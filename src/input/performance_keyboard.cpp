#include "performance_keyboard.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "src/midi/project_transport_timeline.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include "src/platform/cardputer_usb_midi_service.h"
#endif

namespace {
constexpr uint8_t kChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr uint8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint8_t kNaturalMinor[] = {0, 2, 3, 5, 7, 8, 10};
constexpr uint8_t kMinorPentatonic[] = {0, 3, 5, 7, 10};
constexpr uint8_t kDorian[] = {0, 2, 3, 5, 7, 9, 10};

struct ScaleDefinition {
    const uint8_t* intervals;
    uint8_t count;
    const char* name;
};

constexpr ScaleDefinition kScales[] = {
    {kChromatic, static_cast<uint8_t>(sizeof(kChromatic)), "CHROMATIC"},
    {kMajor, static_cast<uint8_t>(sizeof(kMajor)), "MAJOR"},
    {kNaturalMinor, static_cast<uint8_t>(sizeof(kNaturalMinor)), "NAT MINOR"},
    {kMinorPentatonic, static_cast<uint8_t>(sizeof(kMinorPentatonic)), "MIN PENTA"},
    {kDorian, static_cast<uint8_t>(sizeof(kDorian)), "DORIAN"},
};

constexpr char kLowerRow[] = "asdfghjkl";
constexpr char kUpperRow[] = "qwertyuiop";
constexpr char kSeqtrakDrumKeys[] = "asdfghj";
constexpr uint8_t kStrumOptionsMs[] = {0, 8, 16, 24, 36};
constexpr uint8_t kEuclideanPulseOptions[] = {0, 3, 5, 7, 9, 11, 13, 16};
constexpr double kTransportStepEpsilon = 1.0e-4;
constexpr double kTransportScheduleLeadSteps = 0.5;
constexpr uint32_t kGeneratedNoteOnStaleMicros = 12000u;
constexpr uint32_t kMinimumTransportLeadMicros = 1000u;

uint8_t clampMidiNote(int note) {
    if (note < PerformanceKeyboard::kMinNote) return PerformanceKeyboard::kMinNote;
    if (note > PerformanceKeyboard::kMaxNote) return PerformanceKeyboard::kMaxNote;
    return static_cast<uint8_t>(note);
}

uint32_t stepDurationMicrosForBpm(double bpm) {
    if (!std::isfinite(bpm) || bpm < 30.0) bpm = 30.0;
    if (bpm > 300.0) bpm = 300.0;
    double microsPerStep = 15000000.0 / bpm;  // one sixteenth note
    if (microsPerStep < 30000.0) microsPerStep = 30000.0;
    if (microsPerStep > 500000.0) microsPerStep = 500000.0;
    return static_cast<uint32_t>(microsPerStep + 0.5);
}
}  // namespace

char PerformanceKeyboard::normalizeKey(char key) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
}

bool PerformanceKeyboard::isUpperRowKey(char key) {
    key = normalizeKey(key);
    for (char candidate : kUpperRow) {
        if (candidate == '\0') break;
        if (candidate == key) return true;
    }
    return false;
}

bool PerformanceKeyboard::containsKey(const char* keys,
                                      std::size_t count,
                                      char key) {
    if (!keys) return false;
    key = normalizeKey(key);
    for (std::size_t i = 0; i < count; ++i) {
        if (normalizeKey(keys[i]) == key) return true;
    }
    return false;
}

bool PerformanceKeyboard::scaleDegreeForKey(char physicalKey, uint8_t& degree) {
    physicalKey = normalizeKey(physicalKey);
    for (uint8_t i = 0; i < sizeof(kLowerRow) - 1; ++i) {
        if (kLowerRow[i] == physicalKey) {
            degree = i;
            return true;
        }
    }
    for (uint8_t i = 0; i < sizeof(kUpperRow) - 1; ++i) {
        if (kUpperRow[i] == physicalKey) {
            degree = i;
            return true;
        }
    }
    return false;
}

bool PerformanceKeyboard::isPerformanceKey(char physicalKey) {
    uint8_t degree = 0;
    return scaleDegreeForKey(physicalKey, degree);
}

bool PerformanceKeyboard::drumChannelForKey(char physicalKey,
                                            uint8_t& zeroBasedChannel) {
    physicalKey = normalizeKey(physicalKey);
    for (uint8_t i = 0; i < sizeof(kSeqtrakDrumKeys) - 1; ++i) {
        if (kSeqtrakDrumKeys[i] == physicalKey) {
            zeroBasedChannel = i;
            return true;
        }
    }
    return false;
}

bool PerformanceKeyboard::due(uint32_t nowMicros, uint32_t dueMicros) {
    return static_cast<int32_t>(nowMicros - dueMicros) >= 0;
}

uint8_t PerformanceKeyboard::intervalForDegree(PerformanceScale scale,
                                               uint8_t degree) {
    const uint8_t scaleIndex = static_cast<uint8_t>(scale);
    if (scaleIndex >= static_cast<uint8_t>(PerformanceScale::Count)) return 0;
    const ScaleDefinition& definition = kScales[scaleIndex];
    const uint8_t octave = static_cast<uint8_t>(degree / definition.count);
    const uint8_t index = static_cast<uint8_t>(degree % definition.count);
    return static_cast<uint8_t>(octave * 12 + definition.intervals[index]);
}

bool PerformanceKeyboard::noteForKey(char physicalKey, uint8_t& note) const {
    physicalKey = normalizeKey(physicalKey);
    uint8_t degree = 0;
    if (!scaleDegreeForKey(physicalKey, degree)) return false;

    const int manualOffset = isUpperRowKey(physicalKey) ? 12 : 0;
    const int value = static_cast<int>(kRootC2) + octaveShift_ * 12 +
                      manualOffset + intervalForDegree(scale_, degree);
    if (value < kMinNote || value > kMaxNote) return false;
    note = static_cast<uint8_t>(value);
    return true;
}

int PerformanceKeyboard::findHeld(char physicalKey) const {
    physicalKey = normalizeKey(physicalKey);
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i].physicalKey == physicalKey) return static_cast<int>(i);
    }
    return -1;
}

void PerformanceKeyboard::emitNoteOn(const HeldNote& held) {
    router_.route(MusicalEvent{
        MusicalEventType::NoteOn,
        MusicalEventSource::PerformanceKeyboard,
        target_,
        held.channel,
        held.note,
        held.velocity,
    });
}

void PerformanceKeyboard::emitNoteOff(uint8_t note, uint8_t channel) {
    router_.route(MusicalEvent{
        MusicalEventType::NoteOff,
        MusicalEventSource::PerformanceKeyboard,
        target_,
        channel,
        note,
        0,
    });
}

void PerformanceKeyboard::emitPolyNoteOn(const HeldNote& held) {
    router_.route(MusicalEvent{
        MusicalEventType::NoteOn,
        MusicalEventSource::PerformanceKeyboardPoly,
        target_,
        held.channel,
        held.note,
        held.velocity,
    });
}

void PerformanceKeyboard::emitPolyNoteOff(uint8_t note) {
    router_.route(MusicalEvent{
        MusicalEventType::NoteOff,
        MusicalEventSource::PerformanceKeyboardPoly,
        target_,
        0,
        note,
        0,
    });
}

void PerformanceKeyboard::emitAllNotesOff() {
    router_.route(MusicalEvent{
        MusicalEventType::AllNotesOff,
        MusicalEventSource::PerformanceKeyboard,
        target_,
        0,
        0,
        0,
    });
}

void PerformanceKeyboard::routeGenerated(MusicalEventType type,
                                         uint8_t note,
                                         uint8_t velocity,
                                         uint8_t channel) {
    router_.route(MusicalEvent{
        type,
        MusicalEventSource::Arpeggiator,
        target_,
        channel,
        note,
        velocity,
    });
}

void PerformanceKeyboard::serviceHardwareClock() {
#if defined(ARDUINO)
    service(micros());
#endif
}

void PerformanceKeyboard::setTempoBpm(float bpm) {
    if (bpm < 30.0f) bpm = 30.0f;
    if (bpm > 300.0f) bpm = 300.0f;
    tempoBpm_ = bpm;
}

uint32_t PerformanceKeyboard::stepDurationMicros() const {
    return stepDurationMicrosForBpm(tempoBpm_);
}

bool PerformanceKeyboard::scheduleGenerated(MusicalEventType type,
                                            uint8_t note,
                                            uint8_t velocity,
                                            uint32_t dueMicros,
                                            uint8_t channel) {
    for (ScheduledEvent& slot : scheduled_) {
        if (slot.active) continue;
        slot.active = true;
        slot.dueMicros = dueMicros;
        slot.event = MusicalEvent{
            type,
            MusicalEventSource::Arpeggiator,
            target_,
            channel,
            note,
            velocity,
        };
        return true;
    }
    return false;
}

void PerformanceKeyboard::rememberGeneratedOn(uint8_t note) {
    for (std::size_t i = 0; i < generatedNoteCount_; ++i) {
        if (generatedNotes_[i] == note) return;
    }
    if (generatedNoteCount_ < kMaxGeneratedNotes) {
        generatedNotes_[generatedNoteCount_++] = note;
    }
}

void PerformanceKeyboard::forgetGenerated(uint8_t note) {
    for (std::size_t i = 0; i < generatedNoteCount_; ++i) {
        if (generatedNotes_[i] != note) continue;
        for (std::size_t j = i + 1; j < generatedNoteCount_; ++j) {
            generatedNotes_[j - 1] = generatedNotes_[j];
        }
        --generatedNoteCount_;
        generatedNotes_[generatedNoteCount_] = 0;
        return;
    }
}

void PerformanceKeyboard::processScheduled(uint32_t nowMicros) {
    for (ScheduledEvent& slot : scheduled_) {
        if (!slot.active || !due(nowMicros, slot.dueMicros)) continue;
        const MusicalEvent event = slot.event;
        const uint32_t lateness = nowMicros - slot.dueMicros;
        slot = ScheduledEvent{};

        if (event.type == MusicalEventType::NoteOn &&
            lateness > kGeneratedNoteOnStaleMicros) {
            continue;
        }

        router_.route(event);
        if (event.type == MusicalEventType::NoteOn) {
            rememberGeneratedOn(event.note);
        } else if (event.type == MusicalEventType::NoteOff) {
            forgetGenerated(event.note);
        }
    }
}

void PerformanceKeyboard::clearScheduled() {
    for (ScheduledEvent& slot : scheduled_) slot = ScheduledEvent{};
}

void PerformanceKeyboard::stopGeneratedOutput() {
    clearScheduled();
    for (std::size_t i = 0; i < generatedNoteCount_; ++i) {
        routeGenerated(MusicalEventType::NoteOff, generatedNotes_[i], 0);
    }
    generatedNoteCount_ = 0;
}

bool PerformanceKeyboard::stepEngineEnabled() const {
    if (target_ == MusicalEventTarget::Drums) return false;
    return arpeggiatorEnabled_ || ratchetCount_ > 1 || euclideanPulses_ > 0;
}

bool PerformanceKeyboard::transformedPlaybackEnabled() const {
    if (target_ == MusicalEventTarget::Drums) return false;
    return chordMode_ != PerformanceChordMode::Off || strumMs_ > 0 ||
           stepEngineEnabled();
}

bool PerformanceKeyboard::directPolyphonyEnabled() const {
    return voiceMode_ == PerformanceVoiceMode::Poly &&
           target_ != MusicalEventTarget::Drums &&
           !transformedPlaybackEnabled();
}

bool PerformanceKeyboard::polyChordSustainEnabled() const {
    return voiceMode_ == PerformanceVoiceMode::Poly &&
           target_ != MusicalEventTarget::Drums &&
           chordMode_ != PerformanceChordMode::Off &&
           !stepEngineEnabled();
}

bool PerformanceKeyboard::euclideanStepActive(uint8_t step) const {
    if (euclideanPulses_ == 0 || euclideanPulses_ >= kEuclideanSteps) return true;
    const uint8_t rotated = static_cast<uint8_t>(
        (step + euclideanRotation_) % kEuclideanSteps);
    return static_cast<uint8_t>((rotated * euclideanPulses_) % kEuclideanSteps) <
           euclideanPulses_;
}

void PerformanceKeyboard::resetStepClock() {
    stepClockRunning_ = false;
    nextStepMicros_ = 0;
    transportStepClockRunning_ = false;
    transportStepEpoch_ = 0;
    transportStepOrdinal_ = 0;
    transportStepScheduled_ = false;
    transportBlockAnchorValid_ = false;
    transportAnchorBlockSequence_ = 0;
    transportAnchorMicros_ = 0;
    euclideanStep_ = 0;
    arpIndex_ = 0;
    arpAscending_ = true;
}

std::size_t PerformanceKeyboard::buildChord(uint8_t baseNote,
                                            uint8_t* notes,
                                            std::size_t capacity) const {
    if (!notes || capacity == 0) return 0;

    uint8_t intervals[kMaxChordMemoryNotes]{};
    std::size_t intervalCount = 0;
    switch (chordMode_) {
        case PerformanceChordMode::Major:
            intervals[0] = 0; intervals[1] = 4; intervals[2] = 7;
            intervalCount = 3;
            break;
        case PerformanceChordMode::Minor:
            intervals[0] = 0; intervals[1] = 3; intervals[2] = 7;
            intervalCount = 3;
            break;
        case PerformanceChordMode::Fifth:
            intervals[0] = 0; intervals[1] = 7; intervals[2] = 12;
            intervalCount = 3;
            break;
        case PerformanceChordMode::Minor7:
            intervals[0] = 0; intervals[1] = 3; intervals[2] = 7; intervals[3] = 10;
            intervalCount = 4;
            break;
        case PerformanceChordMode::Memory:
            intervalCount = chordMemoryCount_;
            for (std::size_t i = 0; i < intervalCount; ++i) {
                intervals[i] = chordMemoryIntervals_[i];
            }
            if (intervalCount == 0) {
                intervals[0] = 0;
                intervalCount = 1;
            }
            break;
        case PerformanceChordMode::Off:
        case PerformanceChordMode::Count:
        default:
            intervals[0] = 0;
            intervalCount = 1;
            break;
    }

    std::size_t count = 0;
    for (std::size_t i = 0; i < intervalCount && count < capacity; ++i) {
        const uint8_t note = clampMidiNote(
            static_cast<int>(baseNote) + intervals[i]);
        bool duplicate = false;
        for (std::size_t j = 0; j < count; ++j) {
            if (notes[j] == note) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) notes[count++] = note;
    }
    return count;
}

std::size_t PerformanceKeyboard::buildArpPool(uint8_t* notes,
                                              std::size_t capacity) const {
    if (!notes || capacity == 0 || heldCount_ == 0) return 0;
    std::size_t count = 0;
    for (std::size_t heldIndex = 0; heldIndex < heldCount_; ++heldIndex) {
        uint8_t chord[kMaxChordMemoryNotes]{};
        const std::size_t chordCount = buildChord(
            held_[heldIndex].note, chord, kMaxChordMemoryNotes);
        for (std::size_t j = 0; j < chordCount && count < capacity; ++j) {
            bool duplicate = false;
            for (std::size_t existing = 0; existing < count; ++existing) {
                if (notes[existing] == chord[j]) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) notes[count++] = chord[j];
        }
    }
    std::sort(notes, notes + count);
    return count;
}

void PerformanceKeyboard::reconcileDirectPolyChord(uint32_t nowMicros) {
    clearScheduled();

    uint8_t desired[kMaxPolyChordNotes]{};
    const std::size_t desiredCount = buildArpPool(
        desired, kMaxPolyChordNotes);

    auto desiredContains = [&](uint8_t note) {
        for (std::size_t i = 0; i < desiredCount; ++i) {
            if (desired[i] == note) return true;
        }
        return false;
    };

    std::size_t activeIndex = 0;
    while (activeIndex < generatedNoteCount_) {
        const uint8_t note = generatedNotes_[activeIndex];
        if (desiredContains(note)) {
            ++activeIndex;
            continue;
        }
        routeGenerated(MusicalEventType::NoteOff, note, 0);
        forgetGenerated(note);
    }

    if (desiredCount == 0 || heldCount_ == 0) return;

    const uint8_t velocity = held_[heldCount_ - 1].velocity;
    const uint32_t strumMicros = static_cast<uint32_t>(strumMs_) * 1000u;
    std::size_t newNoteIndex = 0;
    for (std::size_t i = 0; i < desiredCount; ++i) {
        bool alreadyActive = false;
        for (std::size_t j = 0; j < generatedNoteCount_; ++j) {
            if (generatedNotes_[j] == desired[i]) {
                alreadyActive = true;
                break;
            }
        }
        if (alreadyActive) continue;

        const uint32_t onAt = nowMicros +
            static_cast<uint32_t>(newNoteIndex) * strumMicros;
        ++newNoteIndex;
        if (onAt == nowMicros) {
            routeGenerated(MusicalEventType::NoteOn, desired[i], velocity);
            rememberGeneratedOn(desired[i]);
        } else if (!scheduleGenerated(MusicalEventType::NoteOn,
                                      desired[i], velocity, onAt)) {
            stopGeneratedOutput();
            return;
        }
    }
}

uint8_t PerformanceKeyboard::selectArpNote(const uint8_t* notes,
                                           std::size_t count) {
    if (!notes || count == 0) return 0;
    if (arpIndex_ >= count) arpIndex_ = 0;

    std::size_t index = arpIndex_;
    switch (arpDirection_) {
        case PerformanceArpDirection::Down:
            index = count - 1 - arpIndex_;
            arpIndex_ = (arpIndex_ + 1) % count;
            break;
        case PerformanceArpDirection::UpDown:
            index = arpIndex_;
            if (count <= 1) {
                arpIndex_ = 0;
            } else if (arpAscending_) {
                if (arpIndex_ + 1 >= count) {
                    arpAscending_ = false;
                    arpIndex_ = count - 2;
                } else {
                    ++arpIndex_;
                }
            } else if (arpIndex_ == 0) {
                arpAscending_ = true;
                arpIndex_ = 1;
            } else {
                --arpIndex_;
            }
            break;
        case PerformanceArpDirection::Up:
        case PerformanceArpDirection::Count:
        default:
            arpIndex_ = (arpIndex_ + 1) % count;
            break;
    }
    return notes[index];
}

void PerformanceKeyboard::emitPerformanceStep(uint32_t stepStartMicros,
                                              uint32_t stepMicros) {
    if (heldCount_ == 0 || !euclideanStepActive(euclideanStep_)) return;

    uint8_t notes[kMaxGeneratedNotes]{};
    std::size_t noteCount = 0;
    if (arpeggiatorEnabled_) {
        uint8_t pool[kMaxGeneratedNotes]{};
        const std::size_t poolCount = buildArpPool(pool, kMaxGeneratedNotes);
        if (poolCount == 0) return;
        notes[0] = selectArpNote(pool, poolCount);
        noteCount = 1;
    } else {
        noteCount = buildChord(held_[heldCount_ - 1].note,
                               notes,
                               kMaxGeneratedNotes);
    }

    const uint8_t velocity = held_[heldCount_ - 1].velocity;
    const uint32_t subStep = stepMicros / ratchetCount_;
    uint32_t strumMicros = static_cast<uint32_t>(strumMs_) * 1000u;
    if (noteCount > 1) {
        const uint32_t usable = subStep > 5000u ? subStep - 5000u : 0u;
        const uint32_t maximumSpacing =
            usable / static_cast<uint32_t>(noteCount - 1u);
        if (strumMicros > maximumSpacing) strumMicros = maximumSpacing;
    }

    for (uint8_t ratchet = 0; ratchet < ratchetCount_; ++ratchet) {
        const uint32_t ratchetStart = stepStartMicros + ratchet * subStep;
        for (std::size_t noteIndex = 0; noteIndex < noteCount; ++noteIndex) {
            const uint32_t onAt = ratchetStart +
                static_cast<uint32_t>(noteIndex) * strumMicros;
            uint32_t gate = (subStep * 3u) / 5u;
            if (gate < 4000u) gate = 4000u;
            uint32_t offAt = onAt + gate;
            const uint32_t hardEnd = ratchetStart + subStep;
            if (due(offAt, hardEnd)) offAt = hardEnd - 1000u;
            if (due(onAt, offAt)) offAt = onAt + 1000u;
            if (!scheduleGenerated(MusicalEventType::NoteOn,
                                   notes[noteIndex], velocity, onAt)) {
                stopGeneratedOutput();
                return;
            }
            if (!scheduleGenerated(MusicalEventType::NoteOff,
                                   notes[noteIndex], 0, offAt)) {
                stopGeneratedOutput();
                return;
            }
        }
    }
}

bool PerformanceKeyboard::serviceTransportStepClock(uint32_t nowMicros) {
    GroovePuterMidi::ProjectTransportBlockSnapshot snapshot{};
    if (!GroovePuterMidi::projectTransportTimeline().trySnapshot(snapshot) ||
        !snapshot.valid || !snapshot.playing || snapshot.bpmQ16 == 0 ||
        snapshot.blockFrames == 0 || snapshot.sampleRate == 0) {
        transportStepClockRunning_ = false;
        transportBlockAnchorValid_ = false;
        return false;
    }

    const double absoluteSteps = snapshot.absoluteSteps();
    if (!std::isfinite(absoluteSteps) || absoluteSteps < 0.0) {
        transportStepClockRunning_ = false;
        transportBlockAnchorValid_ = false;
        return false;
    }

    const uint32_t stepMicros = stepDurationMicrosForBpm(snapshot.bpm());
    const uint32_t blockMicros = static_cast<uint32_t>(
        (1000000ULL * static_cast<uint64_t>(snapshot.blockFrames)) /
        static_cast<uint64_t>(snapshot.sampleRate));
    if (blockMicros == 0) return false;

    if (!transportStepClockRunning_ ||
        transportStepEpoch_ != snapshot.transportEpoch) {
        transportStepClockRunning_ = true;
        transportStepEpoch_ = snapshot.transportEpoch;
        transportStepOrdinal_ = 0;
        transportStepScheduled_ = false;
        transportBlockAnchorValid_ = false;
        arpIndex_ = 0;
        arpAscending_ = true;
    }

#if defined(ARDUINO)
    uint32_t anchorBlockSequence = 0;
    uint32_t anchorPlaybackMicros = 0;
    if (snapshotCardputerUsbMidiBlockAnchor(anchorBlockSequence,
                                             anchorPlaybackMicros)) {
        const int32_t blockDelta = static_cast<int32_t>(
            snapshot.blockSequence - anchorBlockSequence);
        transportAnchorMicros_ = static_cast<uint32_t>(
            static_cast<int64_t>(anchorPlaybackMicros) +
            static_cast<int64_t>(blockDelta) *
                static_cast<int64_t>(blockMicros));
        transportAnchorBlockSequence_ = snapshot.blockSequence;
        transportBlockAnchorValid_ = true;
    }
#endif

    if (!transportBlockAnchorValid_) {
        transportBlockAnchorValid_ = true;
        transportAnchorBlockSequence_ = snapshot.blockSequence;
        transportAnchorMicros_ = nowMicros;
    } else if (transportAnchorBlockSequence_ != snapshot.blockSequence) {
        const int32_t blockDelta = static_cast<int32_t>(
            snapshot.blockSequence - transportAnchorBlockSequence_);
        if (blockDelta <= 0) {
            transportAnchorMicros_ = nowMicros;
        } else {
            const uint32_t predicted = transportAnchorMicros_ +
                static_cast<uint32_t>(blockDelta) * blockMicros;
            const int32_t error = static_cast<int32_t>(nowMicros - predicted);
            const int32_t maximumError = static_cast<int32_t>(blockMicros * 2u);
            transportAnchorMicros_ =
                error < -static_cast<int32_t>(blockMicros) ||
                error > maximumError
                    ? nowMicros
                    : predicted;
        }
        transportAnchorBlockSequence_ = snapshot.blockSequence;
    }

    const uint64_t currentOrdinal = static_cast<uint64_t>(
        std::floor(absoluteSteps + kTransportStepEpsilon));
    const double phaseIntoStep = absoluteSteps -
        static_cast<double>(currentOrdinal);

    if (transportStepScheduled_) {
        if (transportStepOrdinal_ > currentOrdinal) {
            return true;
        }
        if (transportStepOrdinal_ == currentOrdinal &&
            phaseIntoStep < kTransportScheduleLeadSteps) {
            return true;
        }
    }

    const uint64_t nextOrdinal = currentOrdinal + 1u;
    const double stepsUntilBoundary =
        static_cast<double>(nextOrdinal) - absoluteSteps;
    uint32_t dueMicros = transportAnchorMicros_ +
        static_cast<uint32_t>(std::llround(
            stepsUntilBoundary * static_cast<double>(stepMicros)));
    const int32_t leadMicros = static_cast<int32_t>(dueMicros - nowMicros);
    if (leadMicros < -static_cast<int32_t>(kGeneratedNoteOnStaleMicros)) {
        transportStepOrdinal_ = nextOrdinal;
        transportStepScheduled_ = true;
        return true;
    }
    const uint32_t minimumDue = nowMicros + kMinimumTransportLeadMicros;
    if (leadMicros < static_cast<int32_t>(kMinimumTransportLeadMicros)) {
        dueMicros = minimumDue;
    }

    euclideanStep_ = static_cast<uint8_t>(
        nextOrdinal % static_cast<uint64_t>(kEuclideanSteps));
    emitPerformanceStep(dueMicros, stepMicros);
    transportStepOrdinal_ = nextOrdinal;
    transportStepScheduled_ = true;
    return true;
}

void PerformanceKeyboard::service(uint32_t nowMicros) {
    lastServiceMicros_ = nowMicros;
    processScheduled(nowMicros);

    if (!liveInputAllowed() || heldCount_ == 0 || !stepEngineEnabled()) {
        if (heldCount_ == 0 || !stepEngineEnabled()) resetStepClock();
        return;
    }

    if (transportPlaying_) {
        (void)serviceTransportStepClock(nowMicros);
        processScheduled(nowMicros);
        return;
    }

    transportStepClockRunning_ = false;
    transportBlockAnchorValid_ = false;
    transportStepScheduled_ = false;
    const uint32_t stepMicros = stepDurationMicros();
    if (!stepClockRunning_) {
        stepClockRunning_ = true;
        nextStepMicros_ = nowMicros;
    }

    uint8_t catchUp = 0;
    while (due(nowMicros, nextStepMicros_) && catchUp < 4) {
        emitPerformanceStep(nextStepMicros_, stepMicros);
        euclideanStep_ = static_cast<uint8_t>((euclideanStep_ + 1) % kEuclideanSteps);
        nextStepMicros_ += stepMicros;
        ++catchUp;
    }
    if (catchUp == 4 && due(nowMicros, nextStepMicros_)) {
        nextStepMicros_ = nowMicros + stepMicros;
    }
    processScheduled(nowMicros);
}

void PerformanceKeyboard::triggerDirectTransformed(uint32_t nowMicros) {
    if (polyChordSustainEnabled()) {
        reconcileDirectPolyChord(nowMicros);
        return;
    }

    stopGeneratedOutput();
    if (heldCount_ == 0) return;

    uint8_t notes[kMaxGeneratedNotes]{};
    const std::size_t count = buildChord(
        held_[heldCount_ - 1].note, notes, kMaxGeneratedNotes);
    const uint8_t velocity = held_[heldCount_ - 1].velocity;
    const uint32_t strumMicros = static_cast<uint32_t>(strumMs_) * 1000u;
    for (std::size_t i = 0; i < count; ++i) {
        const uint32_t onAt = nowMicros + static_cast<uint32_t>(i) * strumMicros;
        if (onAt == nowMicros) {
            routeGenerated(MusicalEventType::NoteOn, notes[i], velocity);
            rememberGeneratedOn(notes[i]);
        } else if (!scheduleGenerated(MusicalEventType::NoteOn,
                                      notes[i], velocity, onAt)) {
            stopGeneratedOutput();
            return;
        }
    }
}

bool PerformanceKeyboard::keyDown(char physicalKey, uint8_t velocity) {
    serviceHardwareClock();
    physicalKey = normalizeKey(physicalKey);
    if (!isPerformanceKey(physicalKey)) return false;
    if (!noteModeEnabled_) return false;
    if (!enabled_) return true;
    if (findHeld(physicalKey) >= 0) return true;
    if (heldCount_ >= kMaxHeldNotes) {
        panic();
        return true;
    }

    // Cardputer keys do not report pressure. A zero argument means use the
    // current fixed performance velocity. Explicit non-zero MIDI velocities
    // remain available to external/test callers and are clamped to MIDI data.
    if (velocity == 0) velocity = keyVelocity_;
    if (velocity > 127) velocity = 127;

    if (target_ == MusicalEventTarget::Drums) {
        uint8_t drumChannel = 0;
        if (!drumChannelForKey(physicalKey, drumChannel)) return true;
        held_[heldCount_++] = HeldNote{
            physicalKey,
            kSeqtrakDrumNote,
            velocity,
            drumChannel,
        };
        emitNoteOn(held_[heldCount_ - 1]);
        return true;
    }

    uint8_t note = 0;
    if (!noteForKey(physicalKey, note)) return true;
    held_[heldCount_++] = HeldNote{physicalKey, note, velocity, 0};

    if (stepEngineEnabled()) {
        stopGeneratedOutput();
        resetStepClock();
        service(lastServiceMicros_);
    } else if (transformedPlaybackEnabled()) {
        triggerDirectTransformed(lastServiceMicros_);
    } else if (directPolyphonyEnabled()) {
        emitPolyNoteOn(held_[heldCount_ - 1]);
    } else {
        emitNoteOn(held_[heldCount_ - 1]);
    }
    return true;
}

bool PerformanceKeyboard::keyUp(char physicalKey) {
    serviceHardwareClock();
    physicalKey = normalizeKey(physicalKey);
    const int found = findHeld(physicalKey);
    if (found < 0) return false;

    const std::size_t index = static_cast<std::size_t>(found);
    const HeldNote released = held_[index];
    const bool wasActive = index + 1 == heldCount_;
    for (std::size_t i = index + 1; i < heldCount_; ++i) held_[i - 1] = held_[i];
    held_[--heldCount_] = HeldNote{};

    if (target_ == MusicalEventTarget::Drums) {
        emitNoteOff(released.note, released.channel);
        return true;
    }

    if (stepEngineEnabled()) {
        if (heldCount_ == 0) {
            stopGeneratedOutput();
            resetStepClock();
        } else {
            arpIndex_ = 0;
        }
        return true;
    }

    if (transformedPlaybackEnabled()) {
        if (polyChordSustainEnabled()) {
            reconcileDirectPolyChord(lastServiceMicros_);
            return true;
        }
        if (!wasActive) return true;
        stopGeneratedOutput();
        if (heldCount_ > 0) triggerDirectTransformed(lastServiceMicros_);
        return true;
    }

    if (directPolyphonyEnabled()) {
        emitPolyNoteOff(released.note);
        return true;
    }

    // Plain MONO preserves the same physical key lifecycle as a normal MIDI
    // controller. The receiving synth already knows which other NoteOns are
    // still held and owns note priority/legato. Never synthesize a second
    // NoteOn for the previously held key here.
    emitNoteOff(released.note);
    return true;
}

void PerformanceKeyboard::releaseMissingKeys(const char* pressedKeys,
                                             std::size_t pressedCount) {
    serviceHardwareClock();
    if (heldCount_ == 0) return;

    if (target_ == MusicalEventTarget::Drums) {
        std::size_t write = 0;
        for (std::size_t read = 0; read < heldCount_; ++read) {
            if (containsKey(pressedKeys, pressedCount, held_[read].physicalKey)) {
                held_[write++] = held_[read];
            } else {
                emitNoteOff(held_[read].note, held_[read].channel);
            }
        }
        for (std::size_t i = write; i < heldCount_; ++i) held_[i] = HeldNote{};
        heldCount_ = write;
        return;
    }

    if (transformedPlaybackEnabled()) {
        bool changed = false;
        std::size_t write = 0;
        for (std::size_t read = 0; read < heldCount_; ++read) {
            if (containsKey(pressedKeys, pressedCount, held_[read].physicalKey)) {
                held_[write++] = held_[read];
            } else {
                changed = true;
            }
        }
        for (std::size_t i = write; i < heldCount_; ++i) held_[i] = HeldNote{};
        heldCount_ = write;
        if (!changed) return;

        if (stepEngineEnabled()) {
            if (heldCount_ == 0) {
                stopGeneratedOutput();
                resetStepClock();
            } else {
                arpIndex_ = 0;
            }
        } else if (polyChordSustainEnabled()) {
            reconcileDirectPolyChord(lastServiceMicros_);
        } else {
            stopGeneratedOutput();
            if (heldCount_ > 0) triggerDirectTransformed(lastServiceMicros_);
        }
        return;
    }

    if (directPolyphonyEnabled()) {
        std::size_t write = 0;
        for (std::size_t read = 0; read < heldCount_; ++read) {
            if (containsKey(pressedKeys, pressedCount, held_[read].physicalKey)) {
                held_[write++] = held_[read];
            } else {
                emitPolyNoteOff(held_[read].note);
            }
        }
        for (std::size_t i = write; i < heldCount_; ++i) held_[i] = HeldNote{};
        heldCount_ = write;
        return;
    }

    // MONO recovery also preserves physical key ownership: every missing key
    // gets exactly one NoteOff, while keys that remain held are untouched.
    std::size_t write = 0;
    for (std::size_t read = 0; read < heldCount_; ++read) {
        if (containsKey(pressedKeys, pressedCount, held_[read].physicalKey)) {
            held_[write++] = held_[read];
        } else {
            emitNoteOff(held_[read].note);
        }
    }
    for (std::size_t i = write; i < heldCount_; ++i) held_[i] = HeldNote{};
    heldCount_ = write;
}

void PerformanceKeyboard::setEnabled(bool enabled) {
    serviceHardwareClock();
    if (enabled_ == enabled) return;
    if (!enabled) panic();
    enabled_ = enabled;
}

void PerformanceKeyboard::setNoteModeEnabled(bool enabled) {
    serviceHardwareClock();
    if (noteModeEnabled_ == enabled) return;
    if (!enabled) panic();
    noteModeEnabled_ = enabled;
}

void PerformanceKeyboard::setTransportPlaying(bool playing) {
    serviceHardwareClock();
    if (transportPlaying_ == playing) return;
    transportPlaying_ = playing;

    if (stepEngineEnabled()) {
        stopGeneratedOutput();
        resetStepClock();
    }
}

void PerformanceKeyboard::setTarget(MusicalEventTarget target) {
    if (target_ == target) return;
    panic();
    target_ = target;
}

void PerformanceKeyboard::cycleTarget(int direction) {
    constexpr MusicalEventTarget kTargets[] = {
        MusicalEventTarget::SynthA,
        MusicalEventTarget::SynthB,
        MusicalEventTarget::Dx,
        MusicalEventTarget::Drums,
    };
    constexpr int kTargetCount = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));

    int current = 0;
    for (int i = 0; i < kTargetCount; ++i) {
        if (kTargets[i] == target_) {
            current = i;
            break;
        }
    }
    int next = current + direction;
    while (next < 0) next += kTargetCount;
    while (next >= kTargetCount) next -= kTargetCount;
    setTarget(kTargets[next]);
}

const char* PerformanceKeyboard::targetName() const {
    switch (target_) {
        case MusicalEventTarget::SynthA: return "SYNTH A";
        case MusicalEventTarget::SynthB: return "SYNTH B";
        case MusicalEventTarget::Drums: return "DRUMS";
        case MusicalEventTarget::Dx: return "DX";
    }
    return "UNKNOWN";
}

uint8_t PerformanceKeyboard::targetMidiChannel() const {
    switch (target_) {
        case MusicalEventTarget::SynthA: return 8;
        case MusicalEventTarget::SynthB: return 9;
        case MusicalEventTarget::Dx: return 10;
        case MusicalEventTarget::Drums: return 1;
    }
    return 8;
}

void PerformanceKeyboard::setVoiceMode(PerformanceVoiceMode mode) {
    if (mode >= PerformanceVoiceMode::Count || voiceMode_ == mode) return;
    panic();
    voiceMode_ = mode;
}

void PerformanceKeyboard::toggleVoiceMode() {
    setVoiceMode(voiceMode_ == PerformanceVoiceMode::Mono
                     ? PerformanceVoiceMode::Poly
                     : PerformanceVoiceMode::Mono);
}

const char* PerformanceKeyboard::voiceModeName() const {
    switch (voiceMode_) {
        case PerformanceVoiceMode::Mono: return "MONO";
        case PerformanceVoiceMode::Poly: return "POLY";
        case PerformanceVoiceMode::Count: break;
    }
    return "MONO";
}

void PerformanceKeyboard::setVelocity(uint8_t velocity) {
    int value = static_cast<int>(velocity);
    if (value < kMinVelocity) value = kMinVelocity;
    if (value > kMaxVelocity) value = kMaxVelocity;
    value = ((value + (kVelocityStep / 2)) / kVelocityStep) * kVelocityStep;
    if (value < kMinVelocity) value = kMinVelocity;
    if (value > kMaxVelocity) value = kMaxVelocity;
    keyVelocity_ = static_cast<uint8_t>(value);
}

bool PerformanceKeyboard::adjustVelocity(int direction) {
    if (direction == 0) return false;
    int next = static_cast<int>(keyVelocity_) +
               (direction > 0 ? kVelocityStep : -kVelocityStep);
    if (next < kMinVelocity) next = kMinVelocity;
    if (next > kMaxVelocity) next = kMaxVelocity;
    if (next == keyVelocity_) return false;
    keyVelocity_ = static_cast<uint8_t>(next);
    return true;
}

void PerformanceKeyboard::panic() {
    clearScheduled();
    generatedNoteCount_ = 0;
    for (std::size_t i = 0; i < heldCount_; ++i) held_[i] = HeldNote{};
    heldCount_ = 0;
    resetStepClock();
    emitAllNotesOff();
}

void PerformanceKeyboard::setScale(PerformanceScale scale) {
    if (scale >= PerformanceScale::Count || scale_ == scale) return;
    panic();
    scale_ = scale;
}

void PerformanceKeyboard::cycleScale(int direction) {
    int next = static_cast<int>(scale_) + direction;
    const int count = static_cast<int>(PerformanceScale::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    setScale(static_cast<PerformanceScale>(next));
}

const char* PerformanceKeyboard::scaleName() const {
    const uint8_t index = static_cast<uint8_t>(scale_);
    if (index >= static_cast<uint8_t>(PerformanceScale::Count)) return "UNKNOWN";
    return kScales[index].name;
}

bool PerformanceKeyboard::shiftOctave(int direction) {
    int next = octaveShift_ + direction;
    if (next < kMinOctaveShift) next = kMinOctaveShift;
    if (next > kMaxOctaveShift) next = kMaxOctaveShift;
    if (next == octaveShift_) return false;
    panic();
    octaveShift_ = static_cast<int8_t>(next);
    return true;
}

void PerformanceKeyboard::restartAfterConfigurationChange() {
    panic();
}

void PerformanceKeyboard::setChordMode(PerformanceChordMode mode) {
    if (mode >= PerformanceChordMode::Count || chordMode_ == mode) return;
    restartAfterConfigurationChange();
    chordMode_ = mode;
}

void PerformanceKeyboard::cycleChordMode(int direction) {
    int next = static_cast<int>(chordMode_) + direction;
    const int count = static_cast<int>(PerformanceChordMode::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    if (static_cast<PerformanceChordMode>(next) == PerformanceChordMode::Memory &&
        chordMemoryCount_ == 0) {
        next += direction >= 0 ? 1 : -1;
        while (next < 0) next += count;
        while (next >= count) next -= count;
    }
    setChordMode(static_cast<PerformanceChordMode>(next));
}

const char* PerformanceKeyboard::chordModeName() const {
    switch (chordMode_) {
        case PerformanceChordMode::Off: return "OFF";
        case PerformanceChordMode::Major: return "MAJ";
        case PerformanceChordMode::Minor: return "MIN";
        case PerformanceChordMode::Fifth: return "5TH";
        case PerformanceChordMode::Minor7: return "MIN7";
        case PerformanceChordMode::Memory: return "MEM";
        case PerformanceChordMode::Count: break;
    }
    return "OFF";
}

bool PerformanceKeyboard::captureChordMemory() {
    if (heldCount_ == 0) return false;
    uint8_t notes[kMaxHeldNotes]{};
    for (std::size_t i = 0; i < heldCount_; ++i) notes[i] = held_[i].note;
    std::sort(notes, notes + heldCount_);
    const uint8_t root = notes[0];
    chordMemoryCount_ = 0;
    for (std::size_t i = 0; i < heldCount_ && chordMemoryCount_ < kMaxChordMemoryNotes; ++i) {
        const uint8_t interval = static_cast<uint8_t>(notes[i] - root);
        if (chordMemoryCount_ > 0 &&
            chordMemoryIntervals_[chordMemoryCount_ - 1] == interval) {
            continue;
        }
        chordMemoryIntervals_[chordMemoryCount_++] = interval;
    }
    panic();
    chordMode_ = PerformanceChordMode::Memory;
    return chordMemoryCount_ > 0;
}

void PerformanceKeyboard::clearChordMemory() {
    panic();
    for (uint8_t& interval : chordMemoryIntervals_) interval = 0;
    chordMemoryCount_ = 0;
    if (chordMode_ == PerformanceChordMode::Memory) {
        chordMode_ = PerformanceChordMode::Off;
    }
}

void PerformanceKeyboard::setArpeggiatorEnabled(bool enabled) {
    if (arpeggiatorEnabled_ == enabled) return;
    restartAfterConfigurationChange();
    arpeggiatorEnabled_ = enabled;
}

void PerformanceKeyboard::cycleArpDirection(int direction) {
    int next = static_cast<int>(arpDirection_) + direction;
    const int count = static_cast<int>(PerformanceArpDirection::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    restartAfterConfigurationChange();
    arpDirection_ = static_cast<PerformanceArpDirection>(next);
}

const char* PerformanceKeyboard::arpDirectionName() const {
    switch (arpDirection_) {
        case PerformanceArpDirection::Up: return "UP";
        case PerformanceArpDirection::Down: return "DOWN";
        case PerformanceArpDirection::UpDown: return "UPDN";
        case PerformanceArpDirection::Count: break;
    }
    return "UP";
}

void PerformanceKeyboard::cycleStrum(int direction) {
    int current = 0;
    for (int i = 0; i < static_cast<int>(sizeof(kStrumOptionsMs)); ++i) {
        if (kStrumOptionsMs[i] == strumMs_) {
            current = i;
            break;
        }
    }
    int next = current + direction;
    const int count = static_cast<int>(sizeof(kStrumOptionsMs));
    while (next < 0) next += count;
    while (next >= count) next -= count;
    restartAfterConfigurationChange();
    strumMs_ = kStrumOptionsMs[next];
}

void PerformanceKeyboard::cycleRatchet(int direction) {
    int next = static_cast<int>(ratchetCount_) + direction;
    while (next < 1) next += 4;
    while (next > 4) next -= 4;
    restartAfterConfigurationChange();
    ratchetCount_ = static_cast<uint8_t>(next);
}

void PerformanceKeyboard::cycleEuclideanPulses(int direction) {
    int current = 0;
    for (int i = 0; i < static_cast<int>(sizeof(kEuclideanPulseOptions)); ++i) {
        if (kEuclideanPulseOptions[i] == euclideanPulses_) {
            current = i;
            break;
        }
    }
    int next = current + direction;
    const int count = static_cast<int>(sizeof(kEuclideanPulseOptions));
    while (next < 0) next += count;
    while (next >= count) next -= count;
    restartAfterConfigurationChange();
    euclideanPulses_ = kEuclideanPulseOptions[next];
}

void PerformanceKeyboard::rotateEuclidean(int direction) {
    int next = static_cast<int>(euclideanRotation_) + direction;
    while (next < 0) next += kEuclideanSteps;
    while (next >= kEuclideanSteps) next -= kEuclideanSteps;
    restartAfterConfigurationChange();
    euclideanRotation_ = static_cast<uint8_t>(next);
}

int PerformanceKeyboard::activeNote() const {
    return heldCount_ == 0 ? -1 : held_[heldCount_ - 1].note;
}
