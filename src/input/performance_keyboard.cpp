#include "performance_keyboard.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdio>

#include "src/generation/tonal/scale_catalog.h"
#include "src/midi/project_transport_timeline.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include "src/platform/cardputer_usb_midi_service.h"
#endif

namespace {
using GroovePuterRhythm::ScaleDefinitionView;
using GroovePuterRhythm::ScaleTypeValue;

constexpr char kLowerRow[] = "asdfghjkl";
constexpr char kUpperRow[] = "qwertyuiop";
constexpr char kSeqtrakDrumKeys[] = "asdfghj";
constexpr uint8_t kStrumOptionsMs[] = {0, 8, 16, 24, 36};
constexpr uint8_t kGateOptions[] = {25, 50, 60, 75, 95};
constexpr double kTransportStepEpsilon = 1.0e-6;
constexpr double kTransportScheduleLeadPulses = 0.5;
constexpr uint32_t kGeneratedNoteOnStaleMicros = 12000u;
constexpr uint32_t kMinimumTransportLeadMicros = 1000u;

struct PerformanceScaleDefinition {
    ScaleTypeValue catalogValue;
    const char* name;
};

constexpr PerformanceScaleDefinition kPerformanceScales[] = {
    {GroovePuterRhythm::kScaleChromatic, "CHROMATIC"},
    {GroovePuterRhythm::kScaleMajor, "MAJOR"},
    {GroovePuterRhythm::kScaleMinor, "NAT MINOR"},
    {GroovePuterRhythm::kScalePentatonicMinor, "MIN PENTA"},
    {GroovePuterRhythm::kScaleDorian, "DORIAN"},
    {GroovePuterRhythm::kScalePhrygian, "PHRYGIAN"},
    {GroovePuterRhythm::kScaleLydian, "LYDIAN"},
    {GroovePuterRhythm::kScaleMixolydian, "MIXOLYD"},
    {GroovePuterRhythm::kScaleLocrian, "LOCRIAN"},
    {GroovePuterRhythm::kScalePentatonicMajor, "MAJ PENTA"},
};

constexpr const char* kPitchNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

ScaleTypeValue catalogScaleFor(PerformanceScale scale) {
    const uint8_t index = static_cast<uint8_t>(scale);
    if (index >= static_cast<uint8_t>(PerformanceScale::Count)) {
        return GroovePuterRhythm::kScaleChromatic;
    }
    return kPerformanceScales[index].catalogValue;
}

uint8_t fitMidiNote(int note) {
    while (note > PerformanceKeyboard::kMaxNote) note -= 12;
    while (note < PerformanceKeyboard::kMinNote) note += 12;
    return static_cast<uint8_t>(note);
}

std::size_t uniqueInPlace(uint8_t* notes, std::size_t count) {
    if (!notes) return 0;
    std::size_t write = 0;
    for (std::size_t i = 0; i < count; ++i) {
        bool duplicate = false;
        for (std::size_t j = 0; j < write; ++j) {
            if (notes[j] == notes[i]) { duplicate = true; break; }
        }
        if (!duplicate) notes[write++] = notes[i];
    }
    return write;
}

bool stepEnabledFor(const PerformanceClockedConfig& config) {
    return config.arpEnabled || config.ratchetCount > 1 || config.euclideanPulses > 0;
}

uint32_t sixteenthMicrosForBpm(double bpm) {
    if (!std::isfinite(bpm) || bpm < 30.0) bpm = 30.0;
    if (bpm > 300.0) bpm = 300.0;
    return static_cast<uint32_t>(15000000.0 / bpm + 0.5);
}

uint8_t arpPulsesUntilBoundary(std::size_t count,
                              std::size_t index,
                              bool ascending,
                              PerformanceArpDirection direction) {
    if (count <= 1) return 1;
    if (direction != PerformanceArpDirection::UpDown &&
        direction != PerformanceArpDirection::DownUp) {
        const std::size_t remaining = index < count ? count - index : count;
        return static_cast<uint8_t>(remaining > 255u ? 255u : remaining);
    }

    if (index >= count) index = 0;
    const bool atBoundary = index == 0 && ascending;
    const std::size_t cycleLength = count * 2u - 2u;
    if (atBoundary) return static_cast<uint8_t>(cycleLength > 255u ? 255u : cycleLength);

    for (std::size_t pulse = 1; pulse <= cycleLength; ++pulse) {
        if (ascending) {
            if (index + 1u >= count) {
                ascending = false;
                index = count - 2u;
            } else {
                ++index;
            }
        } else if (index == 0) {
            ascending = true;
            index = 1;
        } else {
            --index;
        }
        if (index == 0 && ascending) return static_cast<uint8_t>(pulse);
    }
    return static_cast<uint8_t>(cycleLength > 255u ? 255u : cycleLength);
}

bool noteBelongsToScale(uint8_t note,
                        uint8_t rootPitchClass,
                        PerformanceScale scale,
                        uint8_t& degreeIndex) {
    const ScaleDefinitionView definition =
        GroovePuterRhythm::scaleDefinitionFor(catalogScaleFor(scale));
    if (definition.intervals == nullptr || definition.count == 0) return false;
    const uint8_t delta = static_cast<uint8_t>((note % 12u + 12u - rootPitchClass) % 12u);
    for (uint8_t i = 0; i < definition.count; ++i) {
        if (static_cast<uint8_t>(definition.intervals[i]) == delta) {
            degreeIndex = i;
            return true;
        }
    }
    return false;
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
        if (kLowerRow[i] == physicalKey) { degree = i; return true; }
    }
    for (uint8_t i = 0; i < sizeof(kUpperRow) - 1; ++i) {
        if (kUpperRow[i] == physicalKey) { degree = i; return true; }
    }
    return false;
}

bool PerformanceKeyboard::isPerformanceKey(char physicalKey) {
    uint8_t degree = 0;
    return scaleDegreeForKey(physicalKey, degree);
}

void PerformanceKeyboard::syncPerformanceScaleContext() {
    const ScaleDefinitionView definition =
        GroovePuterRhythm::scaleDefinitionFor(catalogScaleFor(scale_));
    uint16_t mask = 0;
    if (definition.intervals != nullptr) {
        for (uint8_t i = 0; i < definition.count; ++i) {
            const uint8_t pitchClass = static_cast<uint8_t>(
                (rootPitchClass_ + static_cast<uint8_t>(definition.intervals[i])) % 12u);
            mask = static_cast<uint16_t>(mask | (1u << pitchClass));
        }
    }
    performanceContext_.scalePitchClasses = mask == 0 ? 0x0fffu : mask;
    if (!performanceContext_.hasHarmony) {
        int root = static_cast<int>(kRootC2) + rootPitchClass_ + octaveShift_ * 12;
        if (root < 0) root = 0;
        if (root > 127) root = 127;
        performanceContext_.rootMidi = static_cast<uint8_t>(root);
    }
}

void PerformanceKeyboard::setPerformanceContext(
    const GroovePuterPerformance::PerformanceContext& context) {
    const bool changed = performanceContext_.hasHarmony != context.hasHarmony ||
        performanceContext_.rootMidi != context.rootMidi ||
        performanceContext_.harmony.degree != context.harmony.degree ||
        performanceContext_.harmony.quality != context.harmony.quality ||
        performanceContext_.harmony.rootOffsetSemitones != context.harmony.rootOffsetSemitones ||
        performanceContext_.scalePitchClasses != context.scalePitchClasses ||
        performanceContext_.allowChromatic != context.allowChromatic ||
        performanceContext_.sourceOrdinal != context.sourceOrdinal ||
        performanceContext_.eventOnset != context.eventOnset ||
        performanceContext_.eventDuration != context.eventDuration;
    if (changed && performanceContext_.hasHarmony && context.hasHarmony &&
        pendingClocked_.arpEnabled &&
        (arpPitchSource_ == GroovePuterPerformance::ArpPitchSource::Harmony ||
         arpPitchSource_ == GroovePuterPerformance::ArpPitchSource::Role)) {
        if (harmonyChangePolicy_ == GroovePuterPerformance::HarmonyChangePolicy::Hold) {
            heldArpContext_ = performanceContext_;
            arpHoldActive_ = true;
            arpHoldPulsesRemaining_ = 0;
        } else if (harmonyChangePolicy_ == GroovePuterPerformance::HarmonyChangePolicy::Restart) {
            arpRestartPending_ = true;
        }
    }
    performanceContext_ = context;
}

GroovePuterPerformance::PerformanceContext
PerformanceKeyboard::noteInterpretationContext() const {
    GroovePuterPerformance::PerformanceContext context = performanceContext_;
    if (hasPreviousPerformedNote_) {
        context.previousNote = previousPerformedNote_;
        context.hasPreviousNote = true;
    }
    return context;
}

void PerformanceKeyboard::rememberPerformedNote(uint8_t note) {
    previousPerformedNote_ = note;
    hasPreviousPerformedNote_ = true;
}

void PerformanceKeyboard::clearPerformanceHarmonyContext() {
    const bool hadHarmony = performanceContext_.hasHarmony;
    performanceContext_.hasHarmony = false;
    performanceContext_.harmony = GroovePuterRhythm::HarmonicEvent{};
    performanceContext_.sourceOrdinal = 0;
    performanceContext_.eventOnset = 0;
    performanceContext_.eventDuration = 0;
    arpHoldActive_ = false;
    arpHoldPulsesRemaining_ = 0;
    if (hadHarmony && harmonyChangePolicy_ == GroovePuterPerformance::HarmonyChangePolicy::Restart &&
        pendingClocked_.arpEnabled &&
        (arpPitchSource_ == GroovePuterPerformance::ArpPitchSource::Harmony ||
         arpPitchSource_ == GroovePuterPerformance::ArpPitchSource::Role)) {
        arpRestartPending_ = true;
    }
    syncPerformanceScaleContext();
}

PerformanceKeyboardConfig PerformanceKeyboard::exportConfiguration() const {
    return PerformanceKeyboardConfig{
        static_cast<uint8_t>(performanceRole_), static_cast<uint8_t>(leadMode_),
        static_cast<uint8_t>(arpPitchSource_), static_cast<uint8_t>(harmonyChangePolicy_),
        static_cast<uint8_t>(pendingClocked_.arpDirection),
        static_cast<uint8_t>(pendingClocked_.rate), pendingClocked_.arpOctaves,
        pendingClocked_.gatePercent, static_cast<uint8_t>(chordMode_), chordInversion_,
        static_cast<uint8_t>(chordSpread_), static_cast<uint8_t>(voiceLeading_),
        rootPitchClass_, static_cast<uint8_t>(scale_), keyVelocity_, octaveShift_,
        static_cast<uint8_t>(pendingClocked_.arpEnabled),
        static_cast<uint8_t>(pendingClocked_.latchEnabled)};
}

bool PerformanceKeyboard::applyConfiguration(const PerformanceKeyboardConfig& config) {
    if (config.role >= static_cast<uint8_t>(GroovePuterPerformance::PerformanceRole::Count) ||
        config.leadMode >= static_cast<uint8_t>(GroovePuterPerformance::LeadMode::Count) ||
        config.arpPitchSource >= static_cast<uint8_t>(GroovePuterPerformance::ArpPitchSource::Count) ||
        config.harmonyChangePolicy >= static_cast<uint8_t>(GroovePuterPerformance::HarmonyChangePolicy::Count) ||
        config.arpDirection >= static_cast<uint8_t>(PerformanceArpDirection::Count) ||
        config.arpRate >= static_cast<uint8_t>(PerformanceRate::Count) ||
        config.arpOctaves < 1u || config.arpOctaves > 4u ||
        config.gatePercent < 1u || config.gatePercent > 100u ||
        config.chordMode >= static_cast<uint8_t>(PerformanceChordMode::Count) ||
        config.chordSpread >= static_cast<uint8_t>(PerformanceSpread::Count) ||
        config.voiceLeading >= static_cast<uint8_t>(PerformanceVoiceLeading::Count) ||
        config.rootPitchClass > 11u ||
        config.scale >= static_cast<uint8_t>(PerformanceScale::Count) ||
        config.velocity < kMinVelocity || config.velocity > kMaxVelocity ||
        config.octaveShift < kMinOctaveShift || config.octaveShift > kMaxOctaveShift ||
        config.arpEnabled > 1u || config.latchEnabled > 1u) return false;

    (void)setPerformanceRole(static_cast<GroovePuterPerformance::PerformanceRole>(config.role));
    (void)setLeadMode(static_cast<GroovePuterPerformance::LeadMode>(config.leadMode));
    (void)setArpPitchSource(static_cast<GroovePuterPerformance::ArpPitchSource>(config.arpPitchSource));
    (void)setHarmonyChangePolicy(static_cast<GroovePuterPerformance::HarmonyChangePolicy>(config.harmonyChangePolicy));
    setScale(static_cast<PerformanceScale>(config.scale));
    setRootPitchClass(config.rootPitchClass);
    octaveShift_ = config.octaveShift;
    setVelocity(config.velocity);
    setChordMode(static_cast<PerformanceChordMode>(config.chordMode));
    setChordInversion(config.chordInversion);
    setChordSpread(static_cast<PerformanceSpread>(config.chordSpread));
    setVoiceLeading(static_cast<PerformanceVoiceLeading>(config.voiceLeading));
    pendingClocked_.arpDirection = static_cast<PerformanceArpDirection>(config.arpDirection);
    pendingClocked_.rate = static_cast<PerformanceRate>(config.arpRate);
    pendingClocked_.arpOctaves = config.arpOctaves;
    pendingClocked_.gatePercent = config.gatePercent;
    pendingClocked_.arpEnabled = config.arpEnabled != 0;
    pendingClocked_.latchEnabled = config.latchEnabled != 0;
    stageClockedConfig();
    persistedConfig_ = exportConfiguration();
    return true;
}

bool PerformanceKeyboard::configChanged() const {
    const PerformanceKeyboardConfig current = exportConfiguration();
    return std::memcmp(&current, &persistedConfig_, sizeof(current)) != 0;
}

void PerformanceKeyboard::markConfigPersisted() {
    persistedConfig_ = exportConfiguration();
}

bool PerformanceKeyboard::setArpPitchSource(
    GroovePuterPerformance::ArpPitchSource source) {
    if (source >= GroovePuterPerformance::ArpPitchSource::Count) return false;
    if (source == arpPitchSource_) return true;
    arpPitchSource_ = source;
    arpIndex_ = 0;
    arpAscending_ = true;
    arpHoldActive_ = false;
    arpHoldPulsesRemaining_ = 0;
    return true;
}

bool PerformanceKeyboard::cycleArpPitchSource(int direction) {
    if (direction == 0) return false;
    constexpr int count = static_cast<int>(GroovePuterPerformance::ArpPitchSource::Count);
    int next = static_cast<int>(arpPitchSource_) + (direction > 0 ? 1 : -1);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    return setArpPitchSource(static_cast<GroovePuterPerformance::ArpPitchSource>(next));
}

const char* PerformanceKeyboard::arpPitchSourceName() const {
    switch (arpPitchSource_) {
        case GroovePuterPerformance::ArpPitchSource::Held: return "HELD";
        case GroovePuterPerformance::ArpPitchSource::Memory: return "MEMORY";
        case GroovePuterPerformance::ArpPitchSource::Harmony: return "HARMONY";
        case GroovePuterPerformance::ArpPitchSource::Role: return "ROLE";
        case GroovePuterPerformance::ArpPitchSource::Count: break;
    }
    return "HELD";
}

bool PerformanceKeyboard::setHarmonyChangePolicy(
    GroovePuterPerformance::HarmonyChangePolicy policy) {
    if (policy >= GroovePuterPerformance::HarmonyChangePolicy::Count) return false;
    if (policy == harmonyChangePolicy_) return true;
    harmonyChangePolicy_ = policy;
    arpHoldActive_ = false;
    arpHoldPulsesRemaining_ = 0;
    arpRestartPending_ = false;
    return true;
}

bool PerformanceKeyboard::cycleHarmonyChangePolicy(int direction) {
    if (direction == 0) return false;
    constexpr int count = static_cast<int>(GroovePuterPerformance::HarmonyChangePolicy::Count);
    int next = static_cast<int>(harmonyChangePolicy_) + (direction > 0 ? 1 : -1);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    return setHarmonyChangePolicy(static_cast<GroovePuterPerformance::HarmonyChangePolicy>(next));
}

const char* PerformanceKeyboard::harmonyChangePolicyName() const {
    switch (harmonyChangePolicy_) {
        case GroovePuterPerformance::HarmonyChangePolicy::Follow: return "FOLLOW";
        case GroovePuterPerformance::HarmonyChangePolicy::Hold: return "HOLD";
        case GroovePuterPerformance::HarmonyChangePolicy::Restart: return "RESTART";
        case GroovePuterPerformance::HarmonyChangePolicy::Count: break;
    }
    return "FOLLOW";
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

uint8_t PerformanceKeyboard::clampPercent(uint8_t value) {
    return value > 100 ? 100 : value;
}

bool PerformanceKeyboard::noteForKey(char physicalKey, uint8_t& note) const {
    physicalKey = normalizeKey(physicalKey);
    uint8_t degree = 0;
    if (!scaleDegreeForKey(physicalKey, degree)) return false;
    const int manualOffset = isUpperRowKey(physicalKey) ? 12 : 0;
    const int scaleOffset = GroovePuterRhythm::scaleDegreeToSemitone(
        catalogScaleFor(scale_), degree);
    const int value = static_cast<int>(kRootC2) + rootPitchClass_ +
                      octaveShift_ * 12 + manualOffset + scaleOffset;
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

int PerformanceKeyboard::findHeldExternal(uint8_t note) const {
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i].physicalKey == '\0' && static_cast<uint8_t>(held_[i].note) == note) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PerformanceKeyboard::drumChannelForExternalNote(uint8_t note, uint8_t& zeroBasedChannel) {
    // Same lane layout as the GM/SEQTRAK drum map used by the built-in keys, by pitch class.
    switch (note % 12) {
        case 0: case 1: zeroBasedChannel = 0; return true;   // kick
        case 2: zeroBasedChannel = 1; return true;           // snare
        case 3: case 4: zeroBasedChannel = 2; return true;   // clap
        case 5: case 6: zeroBasedChannel = 3; return true;   // hat 1
        case 7: case 8: case 10: zeroBasedChannel = 4; return true;  // hat 2 / open hat
        case 9: zeroBasedChannel = 5; return true;           // perc 1
        case 11: zeroBasedChannel = 6; return true;          // perc 2
        default: return false;
    }
}

bool PerformanceKeyboard::isPitchClassHeld(uint8_t pitchClass) const {
    pitchClass %= 12;
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if ((held_[i].note % 12) == pitchClass) return true;
    }
    return false;
}

void PerformanceKeyboard::emitNoteOn(const HeldNote& held) {
    if (held.performanceDirect) {
        for (const ExternalNoteOwner& owner : externalOwners_) {
            if (owner.active && owner.raw && owner.semanticNote == static_cast<uint8_t>(held.note) &&
                owner.semanticChannel == held.channel) return;
        }
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (&held_[i] != &held && held_[i].performanceDirect &&
                held_[i].note == held.note && held_[i].channel == held.channel) return;
        }
    }
    router_.route(MusicalEvent{MusicalEventType::NoteOn,
                               directPolyphonyEnabled()
                                   ? MusicalEventSource::PerformanceKeyboardPoly
                                   : MusicalEventSource::PerformanceKeyboard,
                               target_, held.channel, held.note, held.velocity});
}
void PerformanceKeyboard::emitNoteOff(uint8_t note, uint8_t channel) {
    router_.route(MusicalEvent{MusicalEventType::NoteOff,
                               directPolyphonyEnabled()
                                   ? MusicalEventSource::PerformanceKeyboardPoly
                                   : MusicalEventSource::PerformanceKeyboard,
                               target_, channel, note, 0});
}
void PerformanceKeyboard::emitPolyNoteOn(const HeldNote& held) {
    if (hasExternalRawOwner(static_cast<uint8_t>(held.note), held.channel)) return;
    router_.route(MusicalEvent{MusicalEventType::NoteOn,
                               MusicalEventSource::PerformanceKeyboardPoly,
                               target_, held.channel, held.note, held.velocity});
}
void PerformanceKeyboard::emitPolyNoteOff(uint8_t note) {
    router_.route(MusicalEvent{MusicalEventType::NoteOff,
                               MusicalEventSource::PerformanceKeyboardPoly,
                               target_, 0, note, 0});
}
void PerformanceKeyboard::emitAllNotesOff() {
    router_.route(MusicalEvent{MusicalEventType::AllNotesOff,
                               MusicalEventSource::PerformanceKeyboard,
                               target_, 0, 0, 0});
}
void PerformanceKeyboard::routeGenerated(MusicalEventType type,
                                         uint8_t note,
                                         uint8_t velocity,
                                         uint8_t channel) {
    router_.route(MusicalEvent{type, MusicalEventSource::Arpeggiator,
                               target_, channel, note, velocity});
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

std::size_t PerformanceKeyboard::scheduledDepth() const {
    std::size_t count = 0;
    for (const ScheduledEvent& slot : scheduled_) if (slot.active) ++count;
    return count;
}

void PerformanceKeyboard::resetPerformanceDiagnostics() {
    scheduledHighWater_ = scheduledDepth();
    scheduledOverflowCount_ = 0;
    staleGeneratedNoteOnDrops_ = 0;
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
        slot.event = MusicalEvent{type, MusicalEventSource::Arpeggiator,
                                  target_, channel, note, velocity};
        const std::size_t depth = scheduledDepth();
        if (depth > scheduledHighWater_) scheduledHighWater_ = depth;
        return true;
    }
    ++scheduledOverflowCount_;
    return false;
}

void PerformanceKeyboard::processScheduled(uint32_t nowMicros) {
    for (ScheduledEvent& slot : scheduled_) {
        if (!slot.active || !due(nowMicros, slot.dueMicros)) continue;
        const MusicalEvent event = slot.event;
        const uint32_t lateness = nowMicros - slot.dueMicros;
        slot = ScheduledEvent{};
        if (event.type == MusicalEventType::NoteOn &&
            lateness > kGeneratedNoteOnStaleMicros) {
            ++staleGeneratedNoteOnDrops_;
            continue;
        }
        router_.route(event);
        if (event.type == MusicalEventType::NoteOn) rememberGeneratedOn(event.note);
        else if (event.type == MusicalEventType::NoteOff) forgetGenerated(event.note);
    }
}

void PerformanceKeyboard::clearScheduled() {
    for (ScheduledEvent& slot : scheduled_) slot = ScheduledEvent{};
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

void PerformanceKeyboard::stopGeneratedOutput() {
    clearScheduled();
    for (std::size_t i = 0; i < generatedNoteCount_; ++i) {
        routeGenerated(MusicalEventType::NoteOff, generatedNotes_[i], 0);
    }
    generatedNoteCount_ = 0;
}

bool PerformanceKeyboard::activeStepEngineEnabled() const {
    return target_ != MusicalEventTarget::Drums && stepEnabledFor(activeClocked_);
}
bool PerformanceKeyboard::requestedStepEngineEnabled() const {
    return target_ != MusicalEventTarget::Drums && stepEnabledFor(pendingClocked_);
}
bool PerformanceKeyboard::transformedPlaybackEnabled() const {
    return target_ != MusicalEventTarget::Drums &&
           (chordMode_ != PerformanceChordMode::Off || activeStepEngineEnabled());
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
           !activeStepEngineEnabled();
}

uint32_t PerformanceKeyboard::pulseDurationMicros(PerformanceRate rate,
                                                  double bpm) const {
    const double result = static_cast<double>(sixteenthMicrosForBpm(bpm)) *
                          performancePulseSteps(rate);
    return static_cast<uint32_t>(std::max(1000.0, result) + 0.5);
}

bool PerformanceKeyboard::euclideanPulseActive(uint64_t musicalOrdinal) const {
    const uint8_t length = activeClocked_.euclideanLength == 0
        ? 1 : activeClocked_.euclideanLength;
    const uint8_t pulses = activeClocked_.euclideanPulses;
    if (pulses == 0 || pulses >= length) return true;
    const uint8_t step = static_cast<uint8_t>(musicalOrdinal % length);
    const uint8_t rotated = static_cast<uint8_t>(
        (step + activeClocked_.euclideanRotation) % length);
    return static_cast<uint8_t>((rotated * pulses) % length) < pulses;
}

void PerformanceKeyboard::resetPulseClock(bool resetMusicalPhase) {
    standalonePulseRunning_ = false;
    nextStandalonePulseMicros_ = 0;
    transportPulseClockRunning_ = false;
    transportPulseEpoch_ = 0;
    transportPulseScheduled_ = false;
    transportBlockAnchorValid_ = false;
    transportAnchorBlockSequence_ = 0;
    transportAnchorMicros_ = 0;
    nextTransportPulseProjectStep_ = 0.0;
    rateOriginProjectStep_ = 0.0;
    rateEpochOrdinal_ = 0;
    if (resetMusicalPhase) {
        musicalPulseOrdinal_ = 0;
        arpIndex_ = 0;
        arpAscending_ = true;
    }
}

void PerformanceKeyboard::stageClockedConfig() {
    clockedConfigPending_ = !performanceClockedConfigEqual(activeClocked_, pendingClocked_);
}

void PerformanceKeyboard::captureLatchNow() {
    pendingLatchCount_ = 0;
    if (heldCount_ == 0) {
        pendingLatchCapture_ = true;
        latchReplaceArmed_ = true;
        return;
    }
    for (std::size_t i = 0; i < heldCount_ && pendingLatchCount_ < kMaxLatchedNotes; ++i) {
        bool duplicate = false;
        for (std::size_t j = 0; j < pendingLatchCount_; ++j) {
            if (pendingLatch_[j].note == held_[i].note) { duplicate = true; break; }
        }
        if (!duplicate) {
            pendingLatch_[pendingLatchCount_++] = LatchedNote{held_[i].note, held_[i].velocity};
        }
    }
    pendingLatchCapture_ = true;
    latchReplaceArmed_ = false;
}

void PerformanceKeyboard::commitLatchAtBoundary(
    const PerformanceClockedConfig& oldConfig,
    const PerformanceClockedConfig& newConfig) {
    if (!newConfig.latchEnabled) {
        latchedCount_ = 0;
        pendingLatchCount_ = 0;
        pendingLatchCapture_ = false;
        latchReplaceArmed_ = false;
        return;
    }
    if ((!oldConfig.latchEnabled && newConfig.latchEnabled) || pendingLatchCapture_) {
        latchedCount_ = std::min(pendingLatchCount_, kMaxLatchedNotes);
        for (std::size_t i = 0; i < latchedCount_; ++i) latched_[i] = pendingLatch_[i];
        pendingLatchCapture_ = false;
        if (heldCount_ == 0) latchReplaceArmed_ = true;
    }
}

void PerformanceKeyboard::transitionPlaybackModeAtBoundary(bool oldStepEnabled,
                                                           bool newStepEnabled,
                                                           uint32_t boundaryMicros) {
    if (oldStepEnabled == newStepEnabled) return;
    if (newStepEnabled) {
        if (chordMode_ != PerformanceChordMode::Off) {
            stopGeneratedOutput();
        } else {
            for (std::size_t i = 0; i < heldCount_; ++i) {
                if (voiceMode_ == PerformanceVoiceMode::Poly) emitPolyNoteOff(held_[i].note);
                else emitNoteOff(held_[i].note);
            }
        }
        return;
    }

    stopGeneratedOutput();
    if (heldCount_ == 0) return;
    if (chordMode_ != PerformanceChordMode::Off) {
        triggerDirectTransformed(boundaryMicros);
    } else {
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (voiceMode_ == PerformanceVoiceMode::Poly) emitPolyNoteOn(held_[i]);
            else emitNoteOn(held_[i]);
        }
    }
}

void PerformanceKeyboard::commitPendingClockedConfig(double boundaryProjectStep,
                                                     uint32_t boundaryMicros) {
    const PerformanceClockedConfig oldConfig = activeClocked_;
    const bool oldStep = stepEnabledFor(oldConfig);
    if (clockedConfigPending_) {
        const bool rateChanged = oldConfig.rate != pendingClocked_.rate;
        activeClocked_ = pendingClocked_;
        clockedConfigPending_ = false;
        if (rateChanged) {
            rateOriginProjectStep_ = boundaryProjectStep;
            rateEpochOrdinal_ = 0;
            ++pulseEpoch_;
        }
        if (mutationSeedApplied_ != activeClocked_.mutation.seed) {
            mutationRng_.reset(activeClocked_.mutation.seed);
            mutationSeedApplied_ = activeClocked_.mutation.seed;
        }
    }
    commitLatchAtBoundary(oldConfig, activeClocked_);
    transitionPlaybackModeAtBoundary(oldStep, stepEnabledFor(activeClocked_), boundaryMicros);
}

std::size_t PerformanceKeyboard::buildScaleChord(uint8_t baseNote,
                                                 bool seventh,
                                                 uint8_t* notes,
                                                 std::size_t capacity) const {
    if (!notes || capacity == 0) return 0;
    uint8_t degree = 0;
    if (!noteBelongsToScale(baseNote, rootPitchClass_, scale_, degree)) {
        notes[0] = baseNote;
        return 1;
    }
    const ScaleTypeValue scaleValue = catalogScaleFor(scale_);
    const int rootSemi = GroovePuterRhythm::scaleDegreeToSemitone(scaleValue, degree);
    const uint8_t offsets[] = {0, 2, 4, 6};
    const std::size_t wanted = seventh ? 4 : 3;
    std::size_t count = 0;
    for (std::size_t i = 0; i < wanted && count < capacity; ++i) {
        const int targetSemi = GroovePuterRhythm::scaleDegreeToSemitone(
            scaleValue, static_cast<int>(degree) + offsets[i]);
        notes[count++] = fitMidiNote(static_cast<int>(baseNote) + targetSemi - rootSemi);
    }
    return uniqueInPlace(notes, count);
}

void PerformanceKeyboard::applyInversionAndSpread(uint8_t* notes,
                                                   std::size_t count) const {
    if (!notes || count < 2) return;
    if (chordInversion_ == 0 && chordSpread_ == PerformanceSpread::Close) return;
    std::sort(notes, notes + count);
    const uint8_t inversions = static_cast<uint8_t>(
        std::min<std::size_t>(chordInversion_, count - 1));
    for (uint8_t inv = 0; inv < inversions; ++inv) {
        const uint8_t lifted = fitMidiNote(static_cast<int>(notes[0]) + 12);
        for (std::size_t i = 1; i < count; ++i) notes[i - 1] = notes[i];
        notes[count - 1] = lifted;
        std::sort(notes, notes + count);
    }
    if (chordSpread_ == PerformanceSpread::Wide) {
        for (std::size_t i = 1; i < count; i += 2) {
            notes[i] = fitMidiNote(static_cast<int>(notes[i]) + 12);
        }
        std::sort(notes, notes + count);
    }
}

void PerformanceKeyboard::applyNearestVoiceLeading(uint8_t* notes,
                                                    std::size_t count) const {
    if (!notes || count == 0 || previousVoicingCount_ == 0 ||
        voiceLeading_ != PerformanceVoiceLeading::Nearest ||
        activeClocked_.arpEnabled || pendingClocked_.arpEnabled) {
        return;
    }
    std::sort(notes, notes + count);
    const std::size_t matched = std::min(count, previousVoicingCount_);
    for (std::size_t i = 0; i < matched; ++i) {
        int best = notes[i];
        int bestDistance = std::abs(best - static_cast<int>(previousVoicing_[i]));
        for (int shift = -24; shift <= 24; shift += 12) {
            int candidate = static_cast<int>(notes[i]) + shift;
            if (candidate < kMinNote || candidate > kMaxNote) continue;
            const int distance = std::abs(candidate - static_cast<int>(previousVoicing_[i]));
            if (distance < bestDistance) {
                best = candidate;
                bestDistance = distance;
            }
        }
        notes[i] = static_cast<uint8_t>(best);
    }
    std::sort(notes, notes + count);
}

std::size_t PerformanceKeyboard::buildChord(uint8_t baseNote,
                                            uint8_t* notes,
                                            std::size_t capacity,
                                            bool applyVoiceLeading) const {
    if (!notes || capacity == 0) return 0;
    std::size_t count = 0;
    auto add = [&](int offset) {
        if (count >= capacity) return;
        notes[count++] = fitMidiNote(static_cast<int>(baseNote) + offset);
    };

    switch (chordMode_) {
        case PerformanceChordMode::Major: add(0); add(4); add(7); break;
        case PerformanceChordMode::Minor: add(0); add(3); add(7); break;
        case PerformanceChordMode::Fifth: add(0); add(7); add(12); break;
        case PerformanceChordMode::Minor7: add(0); add(3); add(7); add(10); break;
        case PerformanceChordMode::Sus2: add(0); add(2); add(7); break;
        case PerformanceChordMode::Sus4: add(0); add(5); add(7); break;
        case PerformanceChordMode::Dominant7: add(0); add(4); add(7); add(10); break;
        case PerformanceChordMode::Major7: add(0); add(4); add(7); add(11); break;
        case PerformanceChordMode::ScaleTriad:
            count = buildScaleChord(baseNote, false, notes, capacity);
            break;
        case PerformanceChordMode::ScaleSeventh:
            count = buildScaleChord(baseNote, true, notes, capacity);
            break;
        case PerformanceChordMode::Memory:
            if (chordMemoryCount_ == 0) add(0);
            else {
                for (std::size_t i = 0; i < chordMemoryCount_ && count < capacity; ++i) {
                    add(static_cast<int>(static_cast<int8_t>(chordMemoryIntervals_[i])));
                }
            }
            break;
        case PerformanceChordMode::Off:
        case PerformanceChordMode::Count:
        default: add(0); break;
    }

    count = uniqueInPlace(notes, count);
    applyInversionAndSpread(notes, count);
    if (applyVoiceLeading) applyNearestVoiceLeading(notes, count);
    return uniqueInPlace(notes, count);
}

void PerformanceKeyboard::rememberVoicing(const uint8_t* notes, std::size_t count) {
    if (!notes || count == 0 || voiceLeading_ != PerformanceVoiceLeading::Nearest) return;
    previousVoicingCount_ = std::min(count, kMaxChordMemoryNotes);
    for (std::size_t i = 0; i < previousVoicingCount_; ++i) previousVoicing_[i] = notes[i];
    std::sort(previousVoicing_, previousVoicing_ + previousVoicingCount_);
}

std::size_t PerformanceKeyboard::collectInputPool(LatchedNote* out,
                                                  std::size_t capacity) const {
    if (!out || capacity == 0) return 0;
    std::size_t count = 0;
    if (activeClocked_.latchEnabled && latchedCount_ > 0) {
        for (std::size_t i = 0; i < latchedCount_ && count < capacity; ++i) out[count++] = latched_[i];
        return count;
    }
    for (std::size_t i = 0; i < heldCount_ && count < capacity; ++i) {
        out[count++] = LatchedNote{held_[i].note, held_[i].velocity};
    }
    return count;
}

std::size_t PerformanceKeyboard::buildArpPool(uint8_t* notes,
                                              uint8_t* velocities,
                                              std::size_t capacity) const {
    if (!notes || !velocities || capacity == 0) return 0;
    const GroovePuterPerformance::ArpPitchSource source = arpPitchSource_;
    const GroovePuterPerformance::PerformanceContext& context =
        arpHoldActive_ ? heldArpContext_ : performanceContext_;
    const bool roleUsesHarmony = source == GroovePuterPerformance::ArpPitchSource::Role &&
        performanceRole_ != GroovePuterPerformance::PerformanceRole::Raw &&
        performanceRole_ != GroovePuterPerformance::PerformanceRole::Lead;
    if ((source == GroovePuterPerformance::ArpPitchSource::Harmony || roleUsesHarmony) &&
        context.hasHarmony) {
        uint8_t intervals[5]{};
        uint8_t intervalCount = 0;
        if (!GroovePuterRhythm::chordQualityIntervals(
                context.harmony.quality, intervals, 5, intervalCount)) return 0;
        std::size_t count = 0;
        for (uint8_t i = 0; i < intervalCount; ++i) {
            for (uint8_t octave = 0; octave < activeClocked_.arpOctaves; ++octave) {
                const int candidate = static_cast<int>(context.rootMidi) + intervals[i] +
                                      static_cast<int>(octave) * 12;
                if (candidate < context.minNote || candidate > context.maxNote ||
                    candidate < kMinNote || candidate > kMaxNote) continue;
                const uint8_t pitchClass = static_cast<uint8_t>(candidate % 12);
                if (!context.allowChromatic &&
                    (context.scalePitchClasses & static_cast<uint16_t>(
                        1u << pitchClass)) == 0u) continue;
                bool duplicate = false;
                for (std::size_t j = 0; j < count; ++j) {
                    if (notes[j] == static_cast<uint8_t>(candidate)) duplicate = true;
                }
                if (!duplicate && count < capacity) {
                    notes[count] = static_cast<uint8_t>(candidate);
                    velocities[count++] = keyVelocity_;
                }
            }
        }
        return count;
    }

    if (source == GroovePuterPerformance::ArpPitchSource::Role &&
        performanceRole_ == GroovePuterPerformance::PerformanceRole::Lead) {
        uint16_t allowed = context.scalePitchClasses;
        if (context.hasHarmony &&
            (leadMode_ == GroovePuterPerformance::LeadMode::Chord ||
             leadMode_ == GroovePuterPerformance::LeadMode::VoiceLead)) {
            bool chordValid = false;
            allowed = GroovePuterPerformance::detail::chordPitchClassMask(
                context, chordValid);
            if (!chordValid) return 0;
            if (!context.allowChromatic) {
                allowed = static_cast<uint16_t>(allowed & context.scalePitchClasses);
            }
        }
        std::size_t count = 0;
        for (uint16_t candidate = context.minNote; candidate <= context.maxNote &&
             candidate <= kMaxNote && count < capacity; ++candidate) {
            const uint8_t pc = static_cast<uint8_t>(candidate % 12u);
            if ((allowed & static_cast<uint16_t>(1u << pc)) == 0) continue;
            notes[count] = static_cast<uint8_t>(candidate);
            velocities[count++] = keyVelocity_;
        }
        if (count > 0) return count;
    }

    if (source == GroovePuterPerformance::ArpPitchSource::Memory && chordMemoryCount_ > 0) {
        LatchedNote inputs[kMaxLatchedNotes]{};
        std::size_t inputCount = collectInputPool(inputs, kMaxLatchedNotes);
        if (inputCount == 0) {
            inputs[0] = LatchedNote{context.rootMidi, keyVelocity_};
            inputCount = 1;
        }
        std::size_t count = 0;
        for (std::size_t input = 0; input < inputCount; ++input) {
            for (std::size_t shape = 0; shape < chordMemoryCount_; ++shape) {
                const int interval = static_cast<int8_t>(chordMemoryIntervals_[shape]);
                const uint8_t base = fitMidiNote(static_cast<int>(inputs[input].note) + interval);
                for (uint8_t octave = 0; octave < activeClocked_.arpOctaves; ++octave) {
                    const int candidate = static_cast<int>(base) + static_cast<int>(octave) * 12;
                    if (candidate > kMaxNote) break;
                    bool duplicate = false;
                    for (std::size_t j = 0; j < count; ++j) {
                        if (notes[j] == static_cast<uint8_t>(candidate)) duplicate = true;
                    }
                    if (!duplicate && count < capacity) {
                        notes[count] = static_cast<uint8_t>(candidate);
                        velocities[count++] = inputs[input].velocity;
                    }
                }
            }
        }
        return count;
    }

    LatchedNote inputs[kMaxLatchedNotes]{};
    const std::size_t inputCount = collectInputPool(inputs, kMaxLatchedNotes);
    std::size_t count = 0;
    for (std::size_t input = 0; input < inputCount; ++input) {
        uint8_t chord[kMaxChordMemoryNotes]{};
        const std::size_t chordCount = buildChord(
            inputs[input].note, chord, kMaxChordMemoryNotes, false);
        for (std::size_t j = 0; j < chordCount; ++j) {
            for (uint8_t octave = 0; octave < activeClocked_.arpOctaves; ++octave) {
                const int candidate = static_cast<int>(chord[j]) + octave * 12;
                if (candidate > kMaxNote) break;
                bool duplicate = false;
                for (std::size_t existing = 0; existing < count; ++existing) {
                    if (notes[existing] == static_cast<uint8_t>(candidate)) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate && count < capacity) {
                    notes[count] = static_cast<uint8_t>(candidate);
                    velocities[count] = inputs[input].velocity;
                    ++count;
                }
            }
        }
    }
    return count;
}

uint8_t PerformanceKeyboard::selectArpNote(
    const uint8_t* notes,
    std::size_t count,
    const PerformanceClockedConfig& pulseConfig) {
    if (!notes || count == 0) return 0;
    uint8_t ordered[kMaxGeneratedNotes]{};
    for (std::size_t i = 0; i < count; ++i) ordered[i] = notes[i];
    if (pulseConfig.arpDirection != PerformanceArpDirection::AsPlayed &&
        pulseConfig.arpDirection != PerformanceArpDirection::Random) {
        std::sort(ordered, ordered + count);
    }

    if (pulseConfig.mutation.deviatePercent > 0 &&
        mutationRng_.chance(pulseConfig.mutation.deviatePercent)) {
        return ordered[mutationRng_.next() % count];
    }
    if (pulseConfig.arpDirection == PerformanceArpDirection::Random) {
        return ordered[mutationRng_.next() % count];
    }
    if (arpIndex_ >= count) arpIndex_ = 0;
    std::size_t index = arpIndex_;
    switch (pulseConfig.arpDirection) {
        case PerformanceArpDirection::Down:
            index = count - 1 - arpIndex_;
            arpIndex_ = (arpIndex_ + 1) % count;
            break;
        case PerformanceArpDirection::UpDown:
            index = arpIndex_;
            if (count <= 1) arpIndex_ = 0;
            else if (arpAscending_) {
                if (arpIndex_ + 1 >= count) { arpAscending_ = false; arpIndex_ = count - 2; }
                else ++arpIndex_;
            } else if (arpIndex_ == 0) { arpAscending_ = true; arpIndex_ = 1; }
            else --arpIndex_;
            break;
        case PerformanceArpDirection::DownUp:
            index = count - 1 - arpIndex_;
            if (count <= 1) arpIndex_ = 0;
            else if (arpAscending_) {
                if (arpIndex_ + 1 >= count) { arpAscending_ = false; arpIndex_ = count - 2; }
                else ++arpIndex_;
            } else if (arpIndex_ == 0) { arpAscending_ = true; arpIndex_ = 1; }
            else --arpIndex_;
            break;
        case PerformanceArpDirection::AsPlayed:
        case PerformanceArpDirection::Up:
        case PerformanceArpDirection::Count:
        default:
            index = arpIndex_;
            arpIndex_ = (arpIndex_ + 1) % count;
            break;
    }
    return ordered[index];
}

uint8_t PerformanceKeyboard::velocityForArpNote(const uint8_t* notes,
                                                const uint8_t* velocities,
                                                std::size_t count,
                                                uint8_t selected) const {
    for (std::size_t i = 0; i < count; ++i) if (notes[i] == selected) return velocities[i];
    return keyVelocity_;
}

void PerformanceKeyboard::emitPerformancePulse(
    uint32_t pulseStartMicros,
    uint32_t pulseMicros,
    const PerformanceClockedConfig& pulseConfig) {
    if (!euclideanPulseActive(musicalPulseOrdinal_)) return;
    if (pulseConfig.mutation.skipPercent > 0 &&
        mutationRng_.chance(pulseConfig.mutation.skipPercent)) return;

    LatchedNote inputs[kMaxLatchedNotes]{};
    const std::size_t inputCount = collectInputPool(inputs, kMaxLatchedNotes);
    if (inputCount == 0 && !pulseConfig.arpEnabled) return;

    if (arpRestartPending_) {
        arpIndex_ = 0;
        arpAscending_ = true;
        arpRestartPending_ = false;
    }

    uint8_t notes[kMaxGeneratedNotes]{};
    uint8_t velocities[kMaxGeneratedNotes]{};
    std::size_t noteCount = 0;
    if (pulseConfig.arpEnabled) {
        uint8_t pool[kMaxGeneratedNotes]{};
        uint8_t poolVelocity[kMaxGeneratedNotes]{};
        const std::size_t poolCount = buildArpPool(pool, poolVelocity, kMaxGeneratedNotes);
        if (poolCount == 0) return;
        if (arpHoldActive_ && arpHoldPulsesRemaining_ == 0) {
            arpHoldPulsesRemaining_ = arpPulsesUntilBoundary(
                poolCount, arpIndex_, arpAscending_, pulseConfig.arpDirection);
        }
        uint8_t selected = selectArpNote(pool, poolCount, pulseConfig);
        if (arpHoldActive_ && arpHoldPulsesRemaining_ > 0 &&
            --arpHoldPulsesRemaining_ == 0) {
            arpHoldActive_ = false;
        }
        if (pulseConfig.mutation.octaveJumpPercent > 0 &&
            mutationRng_.chance(pulseConfig.mutation.octaveJumpPercent)) {
            if (selected <= kMaxNote - 12) selected = static_cast<uint8_t>(selected + 12);
            else if (selected >= kMinNote + 12) selected = static_cast<uint8_t>(selected - 12);
        }
        notes[0] = selected;
        velocities[0] = velocityForArpNote(pool, poolVelocity, poolCount,
                                           selected > kMaxNote - 12
                                               ? static_cast<uint8_t>(selected - 12)
                                               : selected);
        noteCount = 1;
    } else {
        noteCount = buildChord(inputs[inputCount - 1].note,
                               notes, kMaxGeneratedNotes, true);
        for (std::size_t i = 0; i < noteCount; ++i) velocities[i] = inputs[inputCount - 1].velocity;
        rememberVoicing(notes, noteCount);
    }

    if (noteCount == 0) return;
    if (!pulseConfig.arpEnabled && noteCount > 1) {
        if (strumDirection_ == PerformanceStrumDirection::HighToLow) {
            std::reverse(notes, notes + noteCount);
            std::reverse(velocities, velocities + noteCount);
        }
    }

    const uint8_t ratchets = pulseConfig.ratchetCount == 0 ? 1 : pulseConfig.ratchetCount;
    const uint32_t subStep = std::max<uint32_t>(1000u, pulseMicros / ratchets);
    uint32_t strumMicros = (!pulseConfig.arpEnabled && chordMode_ != PerformanceChordMode::Off)
        ? static_cast<uint32_t>(strumMs_) * 1000u : 0u;
    if (noteCount > 1 && strumMicros > 0) {
        const uint32_t usable = subStep > 5000u ? subStep - 5000u : 0u;
        const uint32_t maximumSpacing = usable / static_cast<uint32_t>(noteCount - 1u);
        if (strumMicros > maximumSpacing) strumMicros = maximumSpacing;
    }

    for (uint8_t ratchet = 0; ratchet < ratchets; ++ratchet) {
        const uint32_t ratchetStart = pulseStartMicros + ratchet * subStep;
        for (std::size_t noteIndex = 0; noteIndex < noteCount; ++noteIndex) {
            const uint32_t onAt = ratchetStart + static_cast<uint32_t>(noteIndex) * strumMicros;
            uint32_t gate = static_cast<uint32_t>(
                (static_cast<uint64_t>(subStep) * pulseConfig.gatePercent) / 100u);
            if (gate < 4000u) gate = 4000u;
            uint32_t offAt = onAt + gate;
            const uint32_t hardEnd = ratchetStart + subStep;
            if (due(offAt, hardEnd)) offAt = hardEnd > 1000u ? hardEnd - 1000u : hardEnd;
            if (due(onAt, offAt)) offAt = onAt + 1000u;
            if (!scheduleGenerated(MusicalEventType::NoteOn,
                                   notes[noteIndex], velocities[noteIndex], onAt) ||
                !scheduleGenerated(MusicalEventType::NoteOff,
                                   notes[noteIndex], 0, offAt)) {
                stopGeneratedOutput();
                return;
            }
        }
    }
}

bool PerformanceKeyboard::serviceTransportPulseClock(uint32_t nowMicros) {
    GroovePuterMidi::ProjectTransportBlockSnapshot snapshot{};
    if (!GroovePuterMidi::projectTransportTimeline().trySnapshot(snapshot) ||
        !snapshot.valid || !snapshot.playing || snapshot.bpmQ16 == 0 ||
        snapshot.blockFrames == 0 || snapshot.sampleRate == 0) {
        transportPulseClockRunning_ = false;
        transportBlockAnchorValid_ = false;
        return false;
    }
    const double absoluteSteps = snapshot.absoluteSteps();
    if (!std::isfinite(absoluteSteps) || absoluteSteps < 0.0) return false;

    const uint32_t blockMicros = static_cast<uint32_t>(
        (1000000ULL * static_cast<uint64_t>(snapshot.blockFrames)) /
        static_cast<uint64_t>(snapshot.sampleRate));
    if (blockMicros == 0) return false;

    if (!transportPulseClockRunning_ || transportPulseEpoch_ != snapshot.transportEpoch) {
        transportPulseClockRunning_ = true;
        transportPulseEpoch_ = snapshot.transportEpoch;
        transportPulseScheduled_ = false;
        transportBlockAnchorValid_ = false;
        rateEpochOrdinal_ = 0;
        musicalPulseOrdinal_ = 0;
        arpIndex_ = 0;
        arpAscending_ = true;
        const double pulseSteps = performancePulseSteps(activeClocked_.rate);
        const double ordinal = std::floor((absoluteSteps + kTransportStepEpsilon) / pulseSteps);
        nextTransportPulseProjectStep_ = (ordinal + 1.0) * pulseSteps;
        rateOriginProjectStep_ = 0.0;
    }

#if defined(ARDUINO)
    uint32_t anchorBlockSequence = 0;
    uint32_t anchorPlaybackMicros = 0;
    if (snapshotCardputerUsbMidiBlockAnchor(anchorBlockSequence, anchorPlaybackMicros)) {
        const int32_t blockDelta = static_cast<int32_t>(snapshot.blockSequence - anchorBlockSequence);
        transportAnchorMicros_ = static_cast<uint32_t>(
            static_cast<int64_t>(anchorPlaybackMicros) +
            static_cast<int64_t>(blockDelta) * static_cast<int64_t>(blockMicros));
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
        if (blockDelta <= 0) transportAnchorMicros_ = nowMicros;
        else {
            const uint32_t predicted = transportAnchorMicros_ +
                static_cast<uint32_t>(blockDelta) * blockMicros;
            const int32_t error = static_cast<int32_t>(nowMicros - predicted);
            const int32_t maximumError = static_cast<int32_t>(blockMicros * 2u);
            transportAnchorMicros_ =
                error < -static_cast<int32_t>(blockMicros) || error > maximumError
                    ? nowMicros : predicted;
        }
        transportAnchorBlockSequence_ = snapshot.blockSequence;
    }

    if (transportPulseScheduled_) {
        if (absoluteSteps + kTransportStepEpsilon < nextTransportPulseProjectStep_) return true;
        transportPulseScheduled_ = false;
    }

    double stepsUntilBoundary = nextTransportPulseProjectStep_ - absoluteSteps;
    if (stepsUntilBoundary < -kTransportStepEpsilon) {
        const double pulseSteps = performancePulseSteps(activeClocked_.rate);
        while (nextTransportPulseProjectStep_ <= absoluteSteps + kTransportStepEpsilon) {
            nextTransportPulseProjectStep_ += pulseSteps;
            ++rateEpochOrdinal_;
            ++musicalPulseOrdinal_;
        }
        stepsUntilBoundary = nextTransportPulseProjectStep_ - absoluteSteps;
    }

    const double activePulseSteps = performancePulseSteps(activeClocked_.rate);
    if (stepsUntilBoundary > activePulseSteps * kTransportScheduleLeadPulses) return true;

    const uint32_t sixteenthMicros = sixteenthMicrosForBpm(snapshot.bpm());
    uint32_t dueMicros = transportAnchorMicros_ + static_cast<uint32_t>(
        std::llround(stepsUntilBoundary * static_cast<double>(sixteenthMicros)));
    const int32_t leadMicros = static_cast<int32_t>(dueMicros - nowMicros);
    if (leadMicros < -static_cast<int32_t>(kGeneratedNoteOnStaleMicros)) {
        nextTransportPulseProjectStep_ += activePulseSteps;
        ++rateEpochOrdinal_;
        ++musicalPulseOrdinal_;
        return true;
    }
    if (leadMicros < static_cast<int32_t>(kMinimumTransportLeadMicros)) {
        dueMicros = nowMicros + kMinimumTransportLeadMicros;
    }

    const double boundary = nextTransportPulseProjectStep_;
    const PerformanceRate oldRate = activeClocked_.rate;
    commitPendingClockedConfig(boundary, dueMicros);
    const bool rateChanged = oldRate != activeClocked_.rate;
    const PerformanceClockedConfig pulseConfig = activeClocked_;
    const uint32_t pulseMicros = pulseDurationMicros(pulseConfig.rate, snapshot.bpm());
    emitPerformancePulse(dueMicros, pulseMicros, pulseConfig);

    ++musicalPulseOrdinal_;
    ++rateEpochOrdinal_;
    if (rateChanged) nextTransportPulseProjectStep_ = boundary + performancePulseSteps(activeClocked_.rate);
    else nextTransportPulseProjectStep_ = boundary + performancePulseSteps(activeClocked_.rate);
    transportPulseScheduled_ = true;
    return true;
}

void PerformanceKeyboard::service(uint32_t nowMicros) {
    lastServiceMicros_ = nowMicros;
    processScheduled(nowMicros);

    const bool latchedInput = activeClocked_.latchEnabled && latchedCount_ > 0;
    const bool pendingLatchedInput = pendingClocked_.latchEnabled && pendingLatchCount_ > 0;
    const bool hasInput = heldCount_ > 0 || latchedInput || pendingLatchedInput;
    const bool wantsClock = activeStepEngineEnabled() || requestedStepEngineEnabled();
    // A latched arpeggio keeps generating after the PERFORM page was left (see setEnabled).
    const bool generationAllowed =
        noteModeEnabled_ && (enabled_ || latchedArpContinues());
    if (!generationAllowed || !hasInput || !wantsClock) {
        if (!hasInput || !wantsClock) resetPulseClock(false);
        return;
    }

    if (transportPlaying_) {
        (void)serviceTransportPulseClock(nowMicros);
        processScheduled(nowMicros);
        return;
    }

    transportPulseClockRunning_ = false;
    transportBlockAnchorValid_ = false;
    transportPulseScheduled_ = false;
    if (!standalonePulseRunning_) {
        standalonePulseRunning_ = true;
        nextStandalonePulseMicros_ = nowMicros;
    }

    uint8_t catchUp = 0;
    while (due(nowMicros, nextStandalonePulseMicros_) && catchUp < 4) {
        const uint32_t boundary = nextStandalonePulseMicros_;
        commitPendingClockedConfig(0.0, boundary);
        const PerformanceClockedConfig pulseConfig = activeClocked_;
        const uint32_t pulseMicros = pulseDurationMicros(pulseConfig.rate, tempoBpm_);
        emitPerformancePulse(boundary, pulseMicros, pulseConfig);
        ++musicalPulseOrdinal_;
        ++rateEpochOrdinal_;
        nextStandalonePulseMicros_ = boundary + pulseMicros;
        ++catchUp;
    }
    if (catchUp == 4 && due(nowMicros, nextStandalonePulseMicros_)) {
        nextStandalonePulseMicros_ = nowMicros + pulseDurationMicros(activeClocked_.rate, tempoBpm_);
    }
    processScheduled(nowMicros);
}

void PerformanceKeyboard::reconcileDirectPolyChord(uint32_t nowMicros) {
    clearScheduled();
    uint8_t desired[kMaxPolyChordNotes]{};
    std::size_t desiredCount = 0;
    for (std::size_t heldIndex = 0; heldIndex < heldCount_; ++heldIndex) {
        uint8_t chord[kMaxChordMemoryNotes]{};
        const std::size_t chordCount = buildChord(
            held_[heldIndex].note, chord, kMaxChordMemoryNotes, false);
        for (std::size_t j = 0; j < chordCount && desiredCount < kMaxPolyChordNotes; ++j) {
            bool duplicate = false;
            for (std::size_t i = 0; i < desiredCount; ++i) {
                if (desired[i] == chord[j]) { duplicate = true; break; }
            }
            if (!duplicate) desired[desiredCount++] = chord[j];
        }
    }
    std::sort(desired, desired + desiredCount);

    auto desiredContains = [&](uint8_t note) {
        for (std::size_t i = 0; i < desiredCount; ++i) if (desired[i] == note) return true;
        return false;
    };
    std::size_t activeIndex = 0;
    while (activeIndex < generatedNoteCount_) {
        const uint8_t note = generatedNotes_[activeIndex];
        if (desiredContains(note)) { ++activeIndex; continue; }
        routeGenerated(MusicalEventType::NoteOff, note, 0);
        forgetGenerated(note);
    }
    if (desiredCount == 0 || heldCount_ == 0) return;
    const uint8_t velocity = held_[heldCount_ - 1].velocity;
    for (std::size_t i = 0; i < desiredCount; ++i) {
        bool active = false;
        for (std::size_t j = 0; j < generatedNoteCount_; ++j) {
            if (generatedNotes_[j] == desired[i]) { active = true; break; }
        }
        if (!active) {
            routeGenerated(MusicalEventType::NoteOn, desired[i], velocity);
            rememberGeneratedOn(desired[i]);
        }
    }
    (void)nowMicros;
}

void PerformanceKeyboard::triggerDirectTransformed(uint32_t nowMicros) {
    if (polyChordSustainEnabled()) {
        reconcileDirectPolyChord(nowMicros);
        return;
    }
    stopGeneratedOutput();
    if (heldCount_ == 0) return;
    uint8_t notes[kMaxGeneratedNotes]{};
    std::size_t count = buildChord(held_[heldCount_ - 1].note,
                                   notes, kMaxGeneratedNotes, true);
    if (count == 0) return;
    rememberVoicing(notes, count);
    if (strumDirection_ == PerformanceStrumDirection::HighToLow) {
        std::reverse(notes, notes + count);
    }
    const uint8_t velocity = held_[heldCount_ - 1].velocity;
    const uint32_t spacing = static_cast<uint32_t>(strumMs_) * 1000u;
    for (std::size_t i = 0; i < count; ++i) {
        const uint32_t onAt = nowMicros + static_cast<uint32_t>(i) * spacing;
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

void PerformanceKeyboard::revoiceDirectHarmony(uint32_t nowMicros) {
    if (activeStepEngineEnabled() || requestedStepEngineEnabled()) return;
    if (chordMode_ == PerformanceChordMode::Off) return;
    if (polyChordSustainEnabled()) reconcileDirectPolyChord(nowMicros);
    else triggerDirectTransformed(nowMicros);
}

bool PerformanceKeyboard::keyDown(char physicalKey, uint8_t velocity) {
    physicalKey = normalizeKey(physicalKey);
    uint8_t note = 0;
    (void)noteForKey(physicalKey, note);
    const PerformanceInputIdentity input{
        0, kPerformanceInputSourceCardputer,
        kPerformanceInputChannelKeyMatrix,
        static_cast<uint8_t>(physicalKey)};
    return dispatchIntent(makeNotePerformanceIntent(
        PerformanceIntentKind::PlayNote, input, note, velocity));
}

bool PerformanceKeyboard::keyDownLocal_(char physicalKey, uint8_t velocity) {
    serviceHardwareClock();
    physicalKey = normalizeKey(physicalKey);
    if (!isPerformanceKey(physicalKey)) return false;
    if (!noteModeEnabled_) return false;
    if (!enabled_) return true;
    if (findHeld(physicalKey) >= 0) return true;
    if (heldCount_ >= kMaxHeldNotes) { panic(); return true; }
    if (velocity == 0) velocity = keyVelocity_;
    if (velocity > 127) velocity = 127;

    if (target_ == MusicalEventTarget::Drums) {
        uint8_t drumChannel = 0;
        if (!drumChannelForKey(physicalKey, drumChannel)) return true;
        held_[heldCount_++] = HeldNote{physicalKey, kSeqtrakDrumNote, velocity, drumChannel};
        emitNoteOn(held_[heldCount_ - 1]);
        return true;
    }

    uint8_t note = 0;
    if (!noteForKey(physicalKey, note)) return true;
    if (performanceRoleSelected_ &&
        (performanceRole_ == GroovePuterPerformance::PerformanceRole::Bass ||
        performanceRole_ == GroovePuterPerformance::PerformanceRole::Lead ||
        (performanceRole_ == GroovePuterPerformance::PerformanceRole::Raw &&
         chordMode_ == PerformanceChordMode::Off && !activeStepEngineEnabled() &&
         !requestedStepEngineEnabled()))) {
        GroovePuterPerformance::BassFunction bassFunction =
            GroovePuterPerformance::BassFunction::Root;
        if (performanceRole_ == GroovePuterPerformance::PerformanceRole::Bass) {
            uint8_t degree = 0;
            (void)scaleDegreeForKey(physicalKey, degree);
            constexpr GroovePuterPerformance::BassFunction kBassGestures[7] = {
                GroovePuterPerformance::BassFunction::Root,
                GroovePuterPerformance::BassFunction::Third,
                GroovePuterPerformance::BassFunction::Fifth,
                GroovePuterPerformance::BassFunction::Seventh,
                GroovePuterPerformance::BassFunction::Approach,
                GroovePuterPerformance::BassFunction::Fifth,
                GroovePuterPerformance::BassFunction::Third};
            bassFunction = kBassGestures[degree % 7u];
        }
        const GroovePuterPerformance::NotePlan plan = GroovePuterPerformance::interpretNote(
            performanceRole_, leadMode_, noteInterpretationContext(),
            GroovePuterPerformance::PerformanceGesture{note, velocity, bassFunction});
        if (plan.status != GroovePuterPerformance::NotePlanStatus::Ok || plan.count != 1u) return true;
        HeldNote& direct = held_[heldCount_++];
        direct = HeldNote{physicalKey, plan.notes[0].note, plan.notes[0].velocity, 0, true};
        emitNoteOn(direct);
        rememberPerformedNote(plan.notes[0].note);
        return true;
    }

    const std::size_t priorHeld = heldCount_;
    const bool directKeyboardNote = target_ != MusicalEventTarget::Drums &&
        chordMode_ == PerformanceChordMode::Off && !activeStepEngineEnabled() &&
        !requestedStepEngineEnabled() && !directPolyphonyEnabled();
    held_[heldCount_++] = HeldNote{physicalKey, note, velocity, 0, directKeyboardNote};

    if (activeClocked_.latchEnabled || pendingClocked_.latchEnabled) {
        if (!pendingLatchCapture_) {
            pendingLatchCount_ = latchedCount_;
            for (std::size_t i = 0; i < latchedCount_; ++i) pendingLatch_[i] = latched_[i];
        }
        if (priorHeld == 0 && latchReplaceArmed_) {
            pendingLatchCount_ = 0;
            latchReplaceArmed_ = false;
        }
        bool duplicate = false;
        for (std::size_t i = 0; i < pendingLatchCount_; ++i) {
            if (pendingLatch_[i].note == note) { duplicate = true; break; }
        }
        if (!duplicate && pendingLatchCount_ < kMaxLatchedNotes) {
            pendingLatch_[pendingLatchCount_++] = LatchedNote{note, velocity};
        }
        pendingLatchCapture_ = true;
    }

    if (activeStepEngineEnabled() || requestedStepEngineEnabled()) {
        // Active or pending step-engine ownership suppresses an immediate direct
        // attack. Pending activation materializes on the boundary in service().
    } else if (chordMode_ != PerformanceChordMode::Off) {
        triggerDirectTransformed(lastServiceMicros_);
    } else if (directPolyphonyEnabled()) {
        emitPolyNoteOn(held_[heldCount_ - 1]);
    } else {
        emitNoteOn(held_[heldCount_ - 1]);
    }

    if (requestedStepEngineEnabled()) service(lastServiceMicros_);
    return true;
}

bool PerformanceKeyboard::keyUp(char physicalKey) {
    physicalKey = normalizeKey(physicalKey);
    uint8_t note = 0;
    const int found = findHeld(physicalKey);
    if (found >= 0) note = static_cast<uint8_t>(held_[static_cast<std::size_t>(found)].note);
    const PerformanceInputIdentity input{
        0, kPerformanceInputSourceCardputer,
        kPerformanceInputChannelKeyMatrix,
        static_cast<uint8_t>(physicalKey)};
    return dispatchIntent(makeNotePerformanceIntent(
        PerformanceIntentKind::ReleaseNote, input, note, 0));
}

bool PerformanceKeyboard::keyUpLocal_(char physicalKey) {
    serviceHardwareClock();
    physicalKey = normalizeKey(physicalKey);
    const int found = findHeld(physicalKey);
    if (found < 0) return false;
    const std::size_t index = static_cast<std::size_t>(found);
    const HeldNote released = held_[index];
    const bool wasActive = index + 1 == heldCount_;
    for (std::size_t i = index + 1; i < heldCount_; ++i) held_[i - 1] = held_[i];
    held_[--heldCount_] = HeldNote{};
    if (heldCount_ == 0 && (activeClocked_.latchEnabled || pendingClocked_.latchEnabled)) {
        latchReplaceArmed_ = true;
    }

    if (released.performanceDirect) {
        bool anotherOwner = false;
        for (const ExternalNoteOwner& owner : externalOwners_) {
            if (owner.active && owner.raw && owner.semanticNote == static_cast<uint8_t>(released.note) &&
                owner.semanticChannel == released.channel) anotherOwner = true;
        }
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (held_[i].performanceDirect && held_[i].note == released.note &&
                held_[i].channel == released.channel) anotherOwner = true;
        }
        if (!anotherOwner) emitNoteOff(static_cast<uint8_t>(released.note), released.channel);
        return true;
    }
    if (target_ == MusicalEventTarget::Drums) {
        emitNoteOff(released.note, released.channel);
        return true;
    }
    if (activeStepEngineEnabled()) {
        if (heldCount_ == 0 && !(activeClocked_.latchEnabled && latchedCount_ > 0)) {
            stopGeneratedOutput();
            if (!pendingClocked_.latchEnabled) resetPulseClock(false);
        }
        return true;
    }
    if (chordMode_ != PerformanceChordMode::Off) {
        if (polyChordSustainEnabled()) reconcileDirectPolyChord(lastServiceMicros_);
        else if (wasActive) {
            stopGeneratedOutput();
            if (heldCount_ > 0) triggerDirectTransformed(lastServiceMicros_);
        }
        return true;
    }
    if (!hasExternalRawOwner(static_cast<uint8_t>(released.note), released.channel)) {
        if (directPolyphonyEnabled()) emitPolyNoteOff(released.note);
        else emitNoteOff(released.note);
    }
    return true;
}

int PerformanceKeyboard::findExternalOwner(PerformanceInputIdentity input) const {
    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        if (externalOwners_[i].active && samePerformanceInput(externalOwners_[i].input, input)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PerformanceKeyboard::findExternalOwnerForSemantic(uint8_t note, uint8_t channel) const {
    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        const ExternalNoteOwner& owner = externalOwners_[i];
        if (owner.active && owner.semanticNote == note && owner.semanticChannel == channel) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PerformanceKeyboard::hasExternalRawOwner(uint8_t note, uint8_t channel) const {
    for (const ExternalNoteOwner& owner : externalOwners_) {
        if (owner.active && owner.raw && owner.semanticNote == note &&
            owner.semanticChannel == channel) return true;
    }
    return false;
}

bool PerformanceKeyboard::hasLocalDirectOwner(uint8_t note, uint8_t channel) const {
    const bool rawDirectPoly = directPolyphonyEnabled() &&
        chordMode_ == PerformanceChordMode::Off && !activeStepEngineEnabled() &&
        !requestedStepEngineEnabled();
    for (std::size_t i = 0; i < heldCount_; ++i) {
        const HeldNote& held = held_[i];
        if ((held.performanceDirect || rawDirectPoly) &&
            static_cast<uint8_t>(held.note) == note && held.channel == channel) return true;
    }
    return false;
}

void PerformanceKeyboard::removeExternalOwner(std::size_t index) {
    if (index < kMaxExternalNoteOwners) externalOwners_[index] = ExternalNoteOwner{};
}

bool PerformanceKeyboard::externalNoteOn(uint8_t note, uint8_t velocity) {
    return externalLegacyNoteOn(note, velocity,
                                PerformanceInputIdentity{0, 0xffu, 0, note});
}

bool PerformanceKeyboard::externalNoteOff(uint8_t note) {
    return externalNoteOff(note, PerformanceInputIdentity{0, 0xffu, 0, note});
}

bool PerformanceKeyboard::externalNoteOn(uint8_t note,
                                         uint8_t velocity,
                                         PerformanceInputIdentity input) {
    return dispatchIntent(makeNotePerformanceIntent(
        PerformanceIntentKind::PlayNote, input, note, velocity));
}

bool PerformanceKeyboard::dispatchIntent(const PerformanceIntent& intent) {
    switch (intent.kind) {
        case PerformanceIntentKind::PlayNote:
            if (isCardputerKeyInput(intent.input)) {
                return keyDownLocal_(static_cast<char>(intent.input.inputNote), intent.velocity);
            }
            return externalNoteOnIntent_(intent.note, intent.velocity, intent.input);
        case PerformanceIntentKind::ReleaseNote:
            if (isCardputerKeyInput(intent.input)) {
                return keyUpLocal_(static_cast<char>(intent.input.inputNote));
            }
            return externalNoteOffIntent_(intent.note, intent.input);
        case PerformanceIntentKind::Hold:
            externalSustain(true, intent.input);
            return true;
        case PerformanceIntentKind::ReleaseHold:
            externalSustain(false, intent.input);
            return true;
        case PerformanceIntentKind::ChangeOctave:
            return intent.delta != 0 && shiftOctave(intent.delta);
        case PerformanceIntentKind::SelectRole:
            if (intent.value >= static_cast<uint8_t>(GroovePuterPerformance::PerformanceRole::Count))
                return false;
            return setPerformanceRole(
                static_cast<GroovePuterPerformance::PerformanceRole>(intent.value));
        case PerformanceIntentKind::SelectLeadMode:
            if (intent.value >= static_cast<uint8_t>(GroovePuterPerformance::LeadMode::Count))
                return false;
            return setLeadMode(static_cast<GroovePuterPerformance::LeadMode>(intent.value));
        case PerformanceIntentKind::SelectChordMode:
            if (intent.value >= static_cast<uint8_t>(PerformanceChordMode::Count)) return false;
            setChordMode(static_cast<PerformanceChordMode>(intent.value));
            return true;
        case PerformanceIntentKind::ToggleArp:
            if (intent.value > 1u) return false;
            setArpeggiatorEnabled(intent.value != 0u);
            return true;
        case PerformanceIntentKind::SelectArpPitchSource:
            if (intent.value >= static_cast<uint8_t>(GroovePuterPerformance::ArpPitchSource::Count))
                return false;
            return setArpPitchSource(
                static_cast<GroovePuterPerformance::ArpPitchSource>(intent.value));
        case PerformanceIntentKind::SelectArpContour:
            if (intent.value >= static_cast<uint8_t>(PerformanceArpDirection::Count)) return false;
            return setArpDirection(static_cast<PerformanceArpDirection>(intent.value));
        case PerformanceIntentKind::ChangeArpRate:
            if (intent.value >= static_cast<uint8_t>(PerformanceRate::Count)) return false;
            setArpRate(static_cast<PerformanceRate>(intent.value));
            return true;
        case PerformanceIntentKind::SelectHarmonyChangePolicy:
            if (intent.value >= static_cast<uint8_t>(
                    GroovePuterPerformance::HarmonyChangePolicy::Count)) return false;
            return setHarmonyChangePolicy(
                static_cast<GroovePuterPerformance::HarmonyChangePolicy>(intent.value));
        case PerformanceIntentKind::ToggleLatch:
            if (intent.value > 1u) return false;
            if (intent.value != 0u && !arpeggiatorEnabled()) return false;
            setLatchEnabled(intent.value != 0u);
            return true;
        case PerformanceIntentKind::ChangeControl:
        case PerformanceIntentKind::TriggerPerformanceGesture:
        case PerformanceIntentKind::SelectScene:
        case PerformanceIntentKind::CaptureScene:
        case PerformanceIntentKind::RecallScene:
            return false;
    }
    return false;
}

bool PerformanceKeyboard::externalNoteOnIntent_(uint8_t note,
                                                 uint8_t velocity,
                                                 PerformanceInputIdentity input) {
    serviceHardwareClock();
    if (velocity == 0u) return externalNoteOffIntent_(note, input);

    if (performanceRole_ == GroovePuterPerformance::PerformanceRole::Raw ||
        performanceRole_ == GroovePuterPerformance::PerformanceRole::Bass ||
        performanceRole_ == GroovePuterPerformance::PerformanceRole::Lead) {
        const auto context = noteInterpretationContext();
        GroovePuterPerformance::BassFunction bassFunction =
            GroovePuterPerformance::BassFunction::Root;
        if (performanceRole_ == GroovePuterPerformance::PerformanceRole::Bass &&
            context.hasHarmony &&
            !GroovePuterPerformance::bassFunctionForInputNote(
                context, note, bassFunction)) {
            return false;
        }
        const GroovePuterPerformance::NotePlan plan = GroovePuterPerformance::interpretNote(
            performanceRole_, leadMode_, context,
            GroovePuterPerformance::PerformanceGesture{note, velocity, bassFunction});
        if (plan.status != GroovePuterPerformance::NotePlanStatus::Ok || plan.count != 1u) return false;
        if (performanceRole_ == GroovePuterPerformance::PerformanceRole::Raw) {
            return externalRawNoteOn(plan.notes[0].note, plan.notes[0].velocity, input);
        }
        const bool accepted = externalDirectNoteOn(plan.notes[0].note, plan.notes[0].velocity, input);
        if (accepted) rememberPerformedNote(plan.notes[0].note);
        return accepted;
    }

    uint8_t semanticNote = note;
    uint8_t semanticChannel = 0;
    if (target_ == MusicalEventTarget::Drums) {
        if (!drumChannelForExternalNote(note, semanticChannel)) return true;
        semanticNote = kSeqtrakDrumNote;
    } else {
        semanticNote = fitMidiNote(note);
    }

    const int previous = findExternalOwner(input);
    if (previous >= 0) {
        const ExternalNoteOwner& owner = externalOwners_[static_cast<std::size_t>(previous)];
        if (owner.semanticNote == semanticNote && owner.semanticChannel == semanticChannel) return true;
        (void)externalNoteOff(owner.inputNote, input);
    }

    std::size_t freeIndex = kMaxExternalNoteOwners;
    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        if (!externalOwners_[i].active) { freeIndex = i; break; }
    }
    if (freeIndex == kMaxExternalNoteOwners) {
        // The dispatcher is bounded to 24 active notes. This guard handles a stale or misbehaving
        // producer without allowing an ownership table overflow.
        return false;
    }

    if (findExternalOwnerForSemantic(semanticNote, semanticChannel) < 0) {
        if (!externalNoteOnUntracked(note, velocity)) return false;
    }
    externalOwners_[freeIndex] = ExternalNoteOwner{true, input, note, semanticNote, semanticChannel};
    return true;
}

bool PerformanceKeyboard::externalLegacyNoteOn(uint8_t note,
                                               uint8_t velocity,
                                               PerformanceInputIdentity input) {
    serviceHardwareClock();
    if (velocity == 0u) return externalNoteOff(note, input);
    if (!noteModeEnabled_) return false;

    uint8_t semanticNote = note;
    uint8_t semanticChannel = 0;
    if (target_ == MusicalEventTarget::Drums) {
        if (!drumChannelForExternalNote(note, semanticChannel)) return true;
        semanticNote = kSeqtrakDrumNote;
    } else {
        semanticNote = fitMidiNote(note);
    }
    const int existing = findExternalOwner(input);
    if (existing >= 0) {
        const ExternalNoteOwner& owner = externalOwners_[static_cast<std::size_t>(existing)];
        return !owner.raw && owner.semanticNote == semanticNote &&
               owner.semanticChannel == semanticChannel;
    }
    std::size_t freeIndex = kMaxExternalNoteOwners;
    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        if (!externalOwners_[i].active) { freeIndex = i; break; }
    }
    if (freeIndex == kMaxExternalNoteOwners) return false;
    if (findExternalOwnerForSemantic(semanticNote, semanticChannel) < 0 &&
        !externalNoteOnUntracked(note, velocity)) return false;
    externalOwners_[freeIndex] = ExternalNoteOwner{true, input, note, semanticNote, semanticChannel, false};
    return true;
}

bool PerformanceKeyboard::externalNoteOff(uint8_t note, PerformanceInputIdentity input) {
    return dispatchIntent(makeNotePerformanceIntent(
        PerformanceIntentKind::ReleaseNote, input, note, 0));
}

void PerformanceKeyboard::externalAllSoundOff(PerformanceInputIdentity input) {
    serviceHardwareClock();
    if (input.source >= 3u || input.channel >= 16u) return;

    bool sustainChanged = false;
    for (ExternalSustainOwner& owner : externalSustainOwners_) {
        if (!owner.active || owner.generation != input.generation ||
            owner.source != input.source || owner.channel != input.channel ||
            owner.cable != input.cable) continue;
        owner = ExternalSustainOwner{};
        sustainChanged = true;
    }
    if (sustainChanged) {
        bool anyDown = false;
        for (const ExternalSustainOwner& owner : externalSustainOwners_) {
            anyDown = anyDown || owner.active;
        }
        updateExternalSustainAggregate(anyDown);
    }

    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        const ExternalNoteOwner& owner = externalOwners_[i];
        if (!owner.active || owner.input.generation != input.generation ||
            owner.input.source != input.source || owner.input.channel != input.channel ||
            owner.input.cable != input.cable) continue;
        const uint8_t note = owner.inputNote;
        const PerformanceInputIdentity noteInput = owner.input;
        (void)externalNoteOffIntent_(note, noteInput);
    }
}

uint16_t PerformanceKeyboard::effectiveExternalPitchBend() const {
    const ExternalPitchBendOwner* latest = nullptr;
    for (const ExternalPitchBendOwner& owner : externalPitchBendOwners_) {
        if (!owner.active) continue;
        if (latest == nullptr || owner.sequence > latest->sequence) latest = &owner;
    }
    return latest == nullptr ? 8192u : latest->value14;
}

bool PerformanceKeyboard::externalPitchBend(uint16_t value14,
                                            PerformanceInputIdentity input) {
    if (input.source >= 3u || input.channel >= 16u) return false;
    if (value14 > 16383u) value14 = 16383u;
    const uint16_t before = effectiveExternalPitchBend();
    std::size_t found = kMaxExternalPitchBendOwners;
    std::size_t freeIndex = kMaxExternalPitchBendOwners;
    for (std::size_t i = 0; i < kMaxExternalPitchBendOwners; ++i) {
        const ExternalPitchBendOwner& owner = externalPitchBendOwners_[i];
        if (!owner.active) {
            if (freeIndex == kMaxExternalPitchBendOwners) freeIndex = i;
            continue;
        }
        if (owner.input.source == input.source && owner.input.channel == input.channel &&
            owner.input.cable == input.cable) {
            // A new connection generation supersedes every bend from the old session.
            if (owner.input.generation != input.generation) {
                const uint32_t generationDelta = input.generation - owner.input.generation;
                if (generationDelta > 0x7fffffffu) {
                    return true;  // stale message from an older session
                }
                externalPitchBendOwners_[i] = ExternalPitchBendOwner{};
                if (found == i) found = kMaxExternalPitchBendOwners;
                if (freeIndex == kMaxExternalPitchBendOwners) freeIndex = i;
                continue;
            }
            found = i;
        }
    }
    if (value14 == 8192u) {
        if (found < kMaxExternalPitchBendOwners) {
            externalPitchBendOwners_[found] = ExternalPitchBendOwner{};
        }
    } else {
        if (found == kMaxExternalPitchBendOwners) found = freeIndex;
        if (found == kMaxExternalPitchBendOwners) return false;
        ExternalPitchBendOwner& owner = externalPitchBendOwners_[found];
        owner.active = true;
        owner.value14 = value14;
        owner.sequence = ++externalPitchBendSequence_;
        owner.input = input;
    }
    const uint16_t after = effectiveExternalPitchBend();
    if (after != before) {
        router_.route(MusicalEvent{MusicalEventType::PitchBend,
                                  MusicalEventSource::PerformanceKeyboard,
                                  target_, 0, 0, 0, 0, after});
    }
    return true;
}

void PerformanceKeyboard::releaseAllExternalPitchBends() {
    const uint16_t before = effectiveExternalPitchBend();
    for (ExternalPitchBendOwner& owner : externalPitchBendOwners_) {
        owner = ExternalPitchBendOwner{};
    }
    if (before != 8192u) {
        router_.route(MusicalEvent{MusicalEventType::PitchBend,
                                  MusicalEventSource::PerformanceKeyboard,
                                  target_, 0, 0, 0, 0, 8192u});
    }
}

bool PerformanceKeyboard::externalNoteOffIntent_(uint8_t note,
                                                  PerformanceInputIdentity input) {
    serviceHardwareClock();
    const int found = findExternalOwner(input);
    if (found < 0) return false;
    if (externalOwners_[static_cast<std::size_t>(found)].raw) {
        if (hasExternalSustain(input)) {
            externalOwners_[static_cast<std::size_t>(found)].sustainedReleasePending = true;
            return true;
        }
        return externalRawNoteOff(note, input);
    }
    const std::size_t index = static_cast<std::size_t>(found);
    const ExternalNoteOwner released = externalOwners_[index];
    removeExternalOwner(index);
    if (findExternalOwnerForSemantic(released.semanticNote, released.semanticChannel) >= 0) return true;
    (void)note;
    return externalNoteOffUntracked(released.inputNote);
}

bool PerformanceKeyboard::externalRawNoteOn(uint8_t note,
                                            uint8_t velocity,
                                            PerformanceInputIdentity input) {
    return externalDirectNoteOn(note, velocity, input);
}

bool PerformanceKeyboard::externalDirectNoteOn(uint8_t note,
                                               uint8_t velocity,
                                               PerformanceInputIdentity input) {
    serviceHardwareClock();
    if (note > 127u) return false;
    if (velocity == 0u) return externalRawNoteOff(note, input);
    if (!noteModeEnabled_) return false;
    if (!enabled_) return true;

    const int sameInput = findExternalOwner(input);
    if (sameInput >= 0) {
        ExternalNoteOwner& owner = externalOwners_[static_cast<std::size_t>(sameInput)];
        if (owner.raw && owner.semanticNote == note) {
            const bool retrigger = owner.sustainedReleasePending;
            owner.sustainedReleasePending = false;
            if (retrigger) {
                uint32_t scaled = (static_cast<uint32_t>(velocity) * keyVelocity_ +
                                   kDefaultVelocity / 2u) / kDefaultVelocity;
                if (scaled < 1u) scaled = 1u;
                if (scaled > 127u) scaled = 127u;
                router_.route(MusicalEvent{
                    MusicalEventType::NoteOn,
                    directPolyphonyEnabled()
                        ? MusicalEventSource::PerformanceKeyboardPoly
                        : MusicalEventSource::PerformanceKeyboard,
                    target_, owner.semanticChannel, owner.semanticNote,
                    static_cast<uint8_t>(scaled)});
            }
            return true;
        }
        return false;  // one identity cannot own two simultaneous notes
    }

    std::size_t freeIndex = kMaxExternalNoteOwners;
    bool alreadySounding = false;
    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        const ExternalNoteOwner& owner = externalOwners_[i];
        if (!owner.active) {
            if (freeIndex == kMaxExternalNoteOwners) freeIndex = i;
            continue;
        }
        if (owner.raw && owner.semanticNote == note && owner.semanticChannel == 0) {
            alreadySounding = true;
        }
    }
    if (hasLocalDirectOwner(note, 0)) alreadySounding = true;
    if (freeIndex == kMaxExternalNoteOwners) return false;

    uint32_t scaled = (static_cast<uint32_t>(velocity) * keyVelocity_ + kDefaultVelocity / 2) /
                      kDefaultVelocity;
    if (scaled < 1u) scaled = 1u;
    if (scaled > 127u) scaled = 127u;
    if (!alreadySounding) {
        router_.route(MusicalEvent{MusicalEventType::NoteOn,
                                   directPolyphonyEnabled()
                                       ? MusicalEventSource::PerformanceKeyboardPoly
                                       : MusicalEventSource::PerformanceKeyboard,
                                   target_, 0, note, static_cast<uint8_t>(scaled)});
    }
    externalOwners_[freeIndex] = ExternalNoteOwner{true, input, note, note, 0, true};
    return true;
}

bool PerformanceKeyboard::externalRawNoteOff(uint8_t note, PerformanceInputIdentity input) {
    serviceHardwareClock();
    const int found = findExternalOwner(input);
    if (found < 0) return false;
    const std::size_t index = static_cast<std::size_t>(found);
    const ExternalNoteOwner released = externalOwners_[index];
    if (!released.raw) return false;
    removeExternalOwner(index);
    bool anotherOwner = false;
    anotherOwner = hasExternalRawOwner(released.semanticNote, released.semanticChannel) ||
        hasLocalDirectOwner(released.semanticNote, released.semanticChannel);
    if (!anotherOwner) {
        router_.route(MusicalEvent{MusicalEventType::NoteOff,
                                   directPolyphonyEnabled()
                                       ? MusicalEventSource::PerformanceKeyboardPoly
                                       : MusicalEventSource::PerformanceKeyboard,
                                   target_, 0, released.semanticNote, 0});
    }
    (void)note;
    return true;
}

bool PerformanceKeyboard::setPerformanceRole(GroovePuterPerformance::PerformanceRole role) {
    if (role >= GroovePuterPerformance::PerformanceRole::Count) return false;
    performanceRoleSelected_ = true;
    if (performanceRole_ == role) return true;
    releaseAllExternalNotes();
    performanceRole_ = role;
    performanceRoleSelected_ = true;
    resetVoiceLeading();
    hasPreviousPerformedNote_ = false;
    return true;
}

bool PerformanceKeyboard::cyclePerformanceRole(int direction) {
    if (direction == 0) return false;
    const int count = static_cast<int>(GroovePuterPerformance::PerformanceRole::Count);
    int next = static_cast<int>(performanceRole_) + (direction > 0 ? 1 : -1);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    return setPerformanceRole(static_cast<GroovePuterPerformance::PerformanceRole>(next));
}

const char* PerformanceKeyboard::performanceRoleName() const {
    switch (performanceRole_) {
        case GroovePuterPerformance::PerformanceRole::Raw: return "RAW";
        case GroovePuterPerformance::PerformanceRole::Bass: return "BASS";
        case GroovePuterPerformance::PerformanceRole::Lead: return "LEAD";
        case GroovePuterPerformance::PerformanceRole::Chord: return "CHORD";
        case GroovePuterPerformance::PerformanceRole::Arp: return "ARP";
        case GroovePuterPerformance::PerformanceRole::Count: break;
    }
    return "RAW";
}

bool PerformanceKeyboard::setLeadMode(GroovePuterPerformance::LeadMode mode) {
    if (mode >= GroovePuterPerformance::LeadMode::Count) return false;
    if (leadMode_ == mode) return true;
    releaseAllExternalNotes();
    leadMode_ = mode;
    resetVoiceLeading();
    return true;
}

bool PerformanceKeyboard::cycleLeadMode(int direction) {
    if (direction == 0) return false;
    constexpr int count = static_cast<int>(GroovePuterPerformance::LeadMode::Count);
    int next = static_cast<int>(leadMode_) + (direction > 0 ? 1 : -1);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    return setLeadMode(static_cast<GroovePuterPerformance::LeadMode>(next));
}

const char* PerformanceKeyboard::leadModeName() const {
    switch (leadMode_) {
        case GroovePuterPerformance::LeadMode::Raw: return "RAW";
        case GroovePuterPerformance::LeadMode::Scale: return "SCALE";
        case GroovePuterPerformance::LeadMode::Chord: return "CHORD";
        case GroovePuterPerformance::LeadMode::VoiceLead: return "VOICE";
        case GroovePuterPerformance::LeadMode::Count: break;
    }
    return "RAW";
}

bool PerformanceKeyboard::externalNoteOnUntracked(uint8_t note, uint8_t velocity) {
    serviceHardwareClock();
    if (!noteModeEnabled_) return false;
    if (!enabled_) return true;
    if (velocity == 0) return externalNoteOffUntracked(note);
    if (velocity > 127) velocity = 127;
    // The PERFORM velocity setting is a multiplier for an external keyboard (100 = x1.0, the
    // default): the touch dynamics of the keys are kept, only the overall level follows the setting.
    {
        uint32_t scaled = (static_cast<uint32_t>(velocity) * keyVelocity_ + kDefaultVelocity / 2) /
                          kDefaultVelocity;
        if (scaled < 1) scaled = 1;
        if (scaled > 127) scaled = 127;
        velocity = static_cast<uint8_t>(scaled);
    }

    if (target_ == MusicalEventTarget::Drums) {
        uint8_t drumChannel = 0;
        if (!drumChannelForExternalNote(note, drumChannel)) return true;
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (held_[i].physicalKey == '\0' && held_[i].channel == drumChannel) return true;
        }
        if (heldCount_ >= kMaxHeldNotes) { panic(); return true; }
        held_[heldCount_++] = HeldNote{'\0', kSeqtrakDrumNote, velocity, drumChannel};
        emitNoteOn(held_[heldCount_ - 1]);
        return true;
    }

    note = fitMidiNote(note);
    if (findHeldExternal(note) >= 0) return true;
    if (heldCount_ >= kMaxHeldNotes) { panic(); return true; }

    const std::size_t priorHeld = heldCount_;
    held_[heldCount_++] = HeldNote{'\0', note, velocity, 0};

    if (activeClocked_.latchEnabled || pendingClocked_.latchEnabled) {
        if (!pendingLatchCapture_) {
            pendingLatchCount_ = latchedCount_;
            for (std::size_t i = 0; i < latchedCount_; ++i) pendingLatch_[i] = latched_[i];
        }
        if (priorHeld == 0 && latchReplaceArmed_) {
            pendingLatchCount_ = 0;
            latchReplaceArmed_ = false;
        }
        bool duplicate = false;
        for (std::size_t i = 0; i < pendingLatchCount_; ++i) {
            if (pendingLatch_[i].note == note) { duplicate = true; break; }
        }
        if (!duplicate && pendingLatchCount_ < kMaxLatchedNotes) {
            pendingLatch_[pendingLatchCount_++] = LatchedNote{note, velocity};
        }
        pendingLatchCapture_ = true;
    }

    if (activeStepEngineEnabled() || requestedStepEngineEnabled()) {
        // Active or pending step-engine ownership suppresses an immediate direct attack.
    } else if (chordMode_ != PerformanceChordMode::Off) {
        triggerDirectTransformed(lastServiceMicros_);
    } else if (directPolyphonyEnabled()) {
        emitPolyNoteOn(held_[heldCount_ - 1]);
    } else {
        emitNoteOn(held_[heldCount_ - 1]);
    }

    if (requestedStepEngineEnabled()) service(lastServiceMicros_);
    return true;
}

bool PerformanceKeyboard::externalNoteOffUntracked(uint8_t note) {
    serviceHardwareClock();
    int found = -1;
    if (target_ == MusicalEventTarget::Drums) {
        uint8_t drumChannel = 0;
        if (!drumChannelForExternalNote(note, drumChannel)) return false;
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (held_[i].physicalKey == '\0' && held_[i].channel == drumChannel) {
                found = static_cast<int>(i);
                break;
            }
        }
    } else {
        note = fitMidiNote(note);
        found = findHeldExternal(note);
    }
    if (found < 0) return false;
    const std::size_t index = static_cast<std::size_t>(found);
    const HeldNote released = held_[index];
    const bool wasActive = index + 1 == heldCount_;
    for (std::size_t i = index + 1; i < heldCount_; ++i) held_[i - 1] = held_[i];
    held_[--heldCount_] = HeldNote{};
    if (heldCount_ == 0 && (activeClocked_.latchEnabled || pendingClocked_.latchEnabled)) {
        latchReplaceArmed_ = true;
    }

    if (target_ == MusicalEventTarget::Drums) {
        emitNoteOff(released.note, released.channel);
        return true;
    }
    if (activeStepEngineEnabled()) {
        if (heldCount_ == 0 && !(activeClocked_.latchEnabled && latchedCount_ > 0)) {
            stopGeneratedOutput();
            if (!pendingClocked_.latchEnabled) resetPulseClock(false);
        }
        return true;
    }
    if (chordMode_ != PerformanceChordMode::Off) {
        if (polyChordSustainEnabled()) reconcileDirectPolyChord(lastServiceMicros_);
        else if (wasActive) {
            stopGeneratedOutput();
            if (heldCount_ > 0) triggerDirectTransformed(lastServiceMicros_);
        }
        return true;
    }
    if (directPolyphonyEnabled()) emitPolyNoteOff(released.note);
    else emitNoteOff(released.note);
    return true;
}

void PerformanceKeyboard::externalSustain(bool down) {
    serviceHardwareClock();
    if (down == externalSustainDown_) return;
    if (down) {
        latchBeforeExternalSustain_ = latchEnabled();
        externalSustainDown_ = true;
        setLatchEnabled(true);
    } else {
        externalSustainDown_ = false;
        setLatchEnabled(latchBeforeExternalSustain_);
    }
}

bool PerformanceKeyboard::hasExternalSustain(PerformanceInputIdentity input) const {
    for (const ExternalSustainOwner& owner : externalSustainOwners_) {
        if (owner.active && owner.generation == input.generation &&
            owner.source == input.source && owner.channel == input.channel &&
            owner.cable == input.cable) return true;
    }
    return false;
}

void PerformanceKeyboard::updateExternalSustainAggregate(bool down) {
    if (down == externalSustainDown_) return;
    if (down) {
        latchBeforeExternalSustain_ = latchEnabled();
        externalSustainDown_ = true;
        setLatchEnabled(true);
    } else {
        externalSustainDown_ = false;
        setLatchEnabled(latchBeforeExternalSustain_);
    }
}

void PerformanceKeyboard::externalSustain(bool down, PerformanceInputIdentity input) {
    serviceHardwareClock();
    std::size_t freeIndex = kMaxExternalSustainOwners;
    for (std::size_t i = 0; i < kMaxExternalSustainOwners; ++i) {
        ExternalSustainOwner& owner = externalSustainOwners_[i];
        if (owner.active && owner.generation == input.generation &&
            owner.source == input.source && owner.channel == input.channel &&
            owner.cable == input.cable) {
            if (down) return;
            owner = ExternalSustainOwner{};
            freeIndex = i;
            break;
        }
        if (!owner.active && freeIndex == kMaxExternalSustainOwners) freeIndex = i;
    }
    if (down && freeIndex < kMaxExternalSustainOwners) {
        externalSustainOwners_[freeIndex] = ExternalSustainOwner{
            true, input.generation, input.source, input.channel, input.cable};
    }
    if (!down) {
        for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
            const ExternalNoteOwner& owner = externalOwners_[i];
            if (!owner.active || !owner.sustainedReleasePending ||
                owner.input.generation != input.generation || owner.input.source != input.source ||
                owner.input.channel != input.channel || owner.input.cable != input.cable) continue;
            const uint8_t note = owner.inputNote;
            const PerformanceInputIdentity noteInput = owner.input;
            (void)externalRawNoteOff(note, noteInput);
        }
    }
    bool anyDown = false;
    for (const ExternalSustainOwner& owner : externalSustainOwners_) anyDown = anyDown || owner.active;
    updateExternalSustainAggregate(anyDown);
}

std::size_t PerformanceKeyboard::externalHeldCount() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < heldCount_; ++i) count += held_[i].physicalKey == '\0' ? 1 : 0;
    return count;
}

void PerformanceKeyboard::releaseAllExternalNotes() {
    for (ExternalSustainOwner& owner : externalSustainOwners_) owner = ExternalSustainOwner{};
    updateExternalSustainAggregate(false);
    for (std::size_t i = 0; i < kMaxExternalNoteOwners; ++i) {
        if (!externalOwners_[i].active) continue;
        const ExternalNoteOwner owner = externalOwners_[i];
        if (owner.raw) (void)externalRawNoteOff(owner.inputNote, owner.input);
        else (void)externalNoteOff(owner.inputNote, owner.input);
    }
}

void PerformanceKeyboard::releaseMissingKeys(const char* pressedKeys,
                                             std::size_t pressedCount) {
    char missing[kMaxHeldNotes]{};
    std::size_t missingCount = 0;
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i].physicalKey != '\0' &&
            !containsKey(pressedKeys, pressedCount, held_[i].physicalKey)) {
            missing[missingCount++] = held_[i].physicalKey;
        }
    }
    for (std::size_t i = 0; i < missingCount; ++i) keyUp(missing[i]);
}

void PerformanceKeyboard::setEnabled(bool enabled) {
    serviceHardwareClock();
    if (enabled_ == enabled) return;
    if (!enabled) {
        if (latchedArpContinues()) releaseHeldKeepingLatch();
        else panic();
    }
    enabled_ = enabled;
}

// A latched arpeggio is a hands-free voice: leaving the PERFORM page (to tweak a sound, say) must
// not cut it. Everything else keeps the old guard against notes that could no longer be released.
bool PerformanceKeyboard::latchedArpContinues() const {
    return activeClocked_.arpEnabled && activeClocked_.latchEnabled && latchedCount_ > 0;
}

void PerformanceKeyboard::releaseHeldKeepingLatch() {
    char keys[kMaxHeldNotes]{};
    std::size_t keyCount = 0;
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i].physicalKey != '\0') keys[keyCount++] = held_[i].physicalKey;
    }
    // The normal release paths keep the latched notes sounding (heldCount_ == 0 with LATCH).
    for (std::size_t i = 0; i < keyCount; ++i) keyUp(keys[i]);
    releaseAllExternalNotes();
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
    stopGeneratedOutput();
    transportPlaying_ = playing;
    if (activeStepEngineEnabled() || requestedStepEngineEnabled()) resetPulseClock(true);
}

void PerformanceKeyboard::setTarget(MusicalEventTarget target) {
    if (target_ == target) return;
    panic();
    target_ = target;
    resetVoiceLeading();
}
void PerformanceKeyboard::cycleTarget(int direction) {
    constexpr MusicalEventTarget targets[] = {
        MusicalEventTarget::SynthA, MusicalEventTarget::SynthB,
        MusicalEventTarget::Dx, MusicalEventTarget::Drums};
    int current = 0;
    for (int i = 0; i < 4; ++i) if (targets[i] == target_) current = i;
    int next = current + direction;
    while (next < 0) next += 4;
    while (next >= 4) next -= 4;
    setTarget(targets[next]);
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
                     ? PerformanceVoiceMode::Poly : PerformanceVoiceMode::Mono);
}
const char* PerformanceKeyboard::voiceModeName() const {
    return voiceMode_ == PerformanceVoiceMode::Poly ? "POLY" : "MONO";
}

void PerformanceKeyboard::setVelocity(uint8_t velocity) {
    int value = velocity;
    if (value < kMinVelocity) value = kMinVelocity;
    if (value > kMaxVelocity) value = kMaxVelocity;
    value = ((value + kVelocityStep / 2) / kVelocityStep) * kVelocityStep;
    keyVelocity_ = static_cast<uint8_t>(std::max<int>(kMinVelocity,
                                      std::min<int>(kMaxVelocity, value)));
}
bool PerformanceKeyboard::adjustVelocity(int direction) {
    if (direction == 0) return false;
    const uint8_t before = keyVelocity_;
    int next = static_cast<int>(keyVelocity_) + (direction > 0 ? kVelocityStep : -kVelocityStep);
    next = std::max<int>(kMinVelocity, std::min<int>(kMaxVelocity, next));
    keyVelocity_ = static_cast<uint8_t>(next);
    return keyVelocity_ != before;
}

void PerformanceKeyboard::panic() {
    clearScheduled();
    generatedNoteCount_ = 0;
    for (HeldNote& held : held_) held = HeldNote{};
    for (ExternalNoteOwner& owner : externalOwners_) owner = ExternalNoteOwner{};
    for (ExternalSustainOwner& owner : externalSustainOwners_) owner = ExternalSustainOwner{};
    updateExternalSustainAggregate(false);
    heldCount_ = 0;
    latchedCount_ = pendingLatchCount_ = 0;
    pendingLatchCapture_ = false;
    latchReplaceArmed_ = false;
    resetPulseClock(true);
    resetVoiceLeading();
    hasPreviousPerformedNote_ = false;
    emitAllNotesOff();
    releaseAllExternalPitchBends();
}

void PerformanceKeyboard::setScale(PerformanceScale scale) {
    if (scale >= PerformanceScale::Count) return;
    scale_ = scale;
    syncPerformanceScaleContext();
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
    return index < static_cast<uint8_t>(PerformanceScale::Count)
        ? kPerformanceScales[index].name : "UNKNOWN";
}
void PerformanceKeyboard::setRootPitchClass(uint8_t pitchClass) {
    rootPitchClass_ = static_cast<uint8_t>(pitchClass % 12u);
    syncPerformanceScaleContext();
}
void PerformanceKeyboard::cycleRoot(int direction) {
    int next = static_cast<int>(rootPitchClass_) + (direction >= 0 ? 1 : -1);
    while (next < 0) next += 12;
    while (next >= 12) next -= 12;
    setRootPitchClass(static_cast<uint8_t>(next));
}
const char* PerformanceKeyboard::rootName() const { return kPitchNames[rootPitchClass_ % 12u]; }
bool PerformanceKeyboard::shiftOctave(int direction) {
    int next = octaveShift_ + direction;
    next = std::max<int>(kMinOctaveShift, std::min<int>(kMaxOctaveShift, next));
    if (next == octaveShift_) return false;
    octaveShift_ = static_cast<int8_t>(next);
    syncPerformanceScaleContext();
    return true;
}

void PerformanceKeyboard::setChordMode(PerformanceChordMode mode) {
    if (mode >= PerformanceChordMode::Count || chordMode_ == mode) return;
    const PerformanceChordMode old = chordMode_;
    if (activeStepEngineEnabled() || requestedStepEngineEnabled()) {
        chordMode_ = mode;
        return;
    }
    if (old == PerformanceChordMode::Off && heldCount_ > 0) {
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (voiceMode_ == PerformanceVoiceMode::Poly) emitPolyNoteOff(held_[i].note);
            else emitNoteOff(held_[i].note);
        }
    }
    chordMode_ = mode;
    if (mode == PerformanceChordMode::Off) {
        stopGeneratedOutput();
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if (voiceMode_ == PerformanceVoiceMode::Poly) emitPolyNoteOn(held_[i]);
            else emitNoteOn(held_[i]);
        }
    } else {
        revoiceDirectHarmony(lastServiceMicros_);
    }
}
void PerformanceKeyboard::cycleChordMode(int direction) {
    int next = static_cast<int>(chordMode_) + direction;
    const int count = static_cast<int>(PerformanceChordMode::Count);
    do {
        while (next < 0) next += count;
        while (next >= count) next -= count;
        if (static_cast<PerformanceChordMode>(next) != PerformanceChordMode::Memory ||
            chordMemoryCount_ > 0) break;
        next += direction >= 0 ? 1 : -1;
    } while (true);
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
        case PerformanceChordMode::Sus2: return "SUS2";
        case PerformanceChordMode::Sus4: return "SUS4";
        case PerformanceChordMode::Dominant7: return "7";
        case PerformanceChordMode::Major7: return "MAJ7";
        case PerformanceChordMode::ScaleTriad: return "SCALE3";
        case PerformanceChordMode::ScaleSeventh: return "SCALE7";
        case PerformanceChordMode::Count: break;
    }
    return "OFF";
}
void PerformanceKeyboard::setChordInversion(uint8_t inversion) {
    inversion = static_cast<uint8_t>(std::min<int>(3, inversion));
    if (chordInversion_ == inversion) return;
    chordInversion_ = inversion;
    revoiceDirectHarmony(lastServiceMicros_);
}
void PerformanceKeyboard::cycleChordInversion(int direction) {
    int next = static_cast<int>(chordInversion_) + (direction >= 0 ? 1 : -1);
    while (next < 0) next += 4;
    while (next >= 4) next -= 4;
    setChordInversion(static_cast<uint8_t>(next));
}
void PerformanceKeyboard::setChordSpread(PerformanceSpread spread) {
    if (spread >= PerformanceSpread::Count || chordSpread_ == spread) return;
    chordSpread_ = spread;
    revoiceDirectHarmony(lastServiceMicros_);
}
void PerformanceKeyboard::toggleChordSpread() {
    setChordSpread(chordSpread_ == PerformanceSpread::Close
        ? PerformanceSpread::Wide : PerformanceSpread::Close);
}
const char* PerformanceKeyboard::chordSpreadName() const {
    return chordSpread_ == PerformanceSpread::Wide ? "WIDE" : "CLOSE";
}
void PerformanceKeyboard::setVoiceLeading(PerformanceVoiceLeading mode) {
    if (mode >= PerformanceVoiceLeading::Count) return;
    if (mode == PerformanceVoiceLeading::Nearest && pendingClocked_.arpEnabled) return;
    if (voiceLeading_ == mode) return;
    voiceLeading_ = mode;
    revoiceDirectHarmony(lastServiceMicros_);
}
void PerformanceKeyboard::toggleVoiceLeading() {
    setVoiceLeading(voiceLeading_ == PerformanceVoiceLeading::Off
        ? PerformanceVoiceLeading::Nearest : PerformanceVoiceLeading::Off);
}
const char* PerformanceKeyboard::voiceLeadingName() const {
    return voiceLeading_ == PerformanceVoiceLeading::Nearest ? "NEAR" : "OFF";
}
void PerformanceKeyboard::resetVoiceLeading() {
    previousVoicingCount_ = 0;
    for (uint8_t& note : previousVoicing_) note = 0;
}

bool PerformanceKeyboard::captureChordMemory() {
    if (heldCount_ == 0) return false;
    const int anchor = held_[0].note;
    chordMemoryCount_ = 0;
    for (std::size_t i = 0; i < heldCount_ && chordMemoryCount_ < kMaxChordMemoryNotes; ++i) {
        int interval = static_cast<int>(held_[i].note) - anchor;
        interval = std::max(-48, std::min(48, interval));
        chordMemoryIntervals_[chordMemoryCount_++] =
            static_cast<uint8_t>(static_cast<int8_t>(interval));
    }
    setChordMode(PerformanceChordMode::Memory);
    return chordMemoryCount_ > 0;
}
void PerformanceKeyboard::clearChordMemory() {
    chordMemoryCount_ = 0;
    for (uint8_t& interval : chordMemoryIntervals_) interval = 0;
    if (chordMode_ == PerformanceChordMode::Memory) setChordMode(PerformanceChordMode::Off);
}

bool PerformanceKeyboard::formatDetectedChord(char* out, std::size_t capacity) const {
    if (!out || capacity == 0 || heldCount_ == 0 || chordMode_ == PerformanceChordMode::Off) return false;
    uint8_t notes[kMaxChordMemoryNotes]{};
    std::size_t count = buildChord(held_[heldCount_ - 1].note,
                                   notes, kMaxChordMemoryNotes, false);
    if (count == 0) return false;
    std::sort(notes, notes + count);
    const PerformanceChordDetection detection = detectPerformanceChord(notes, count);
    if (!detection.matched) return false;
    const char* inv = detection.inversion == 0 ? "root" :
                      detection.inversion == 1 ? "1st inv" :
                      detection.inversion == 2 ? "2nd inv" : "3rd inv";
    std::snprintf(out, capacity, "%s%s / %s",
                  kPitchNames[detection.rootPitchClass], detection.quality, inv);
    return true;
}

void PerformanceKeyboard::setArpeggiatorEnabled(bool enabled) {
    if (pendingClocked_.arpEnabled == enabled) return;
    pendingClocked_.arpEnabled = enabled;
    if (enabled) {
        strumMs_ = 0;
        voiceLeading_ = PerformanceVoiceLeading::Off;
    } else if (!enabled) {
        pendingClocked_.latchEnabled = false;
    }
    stageClockedConfig();
}
void PerformanceKeyboard::cycleArpDirection(int direction) {
    int next = static_cast<int>(pendingClocked_.arpDirection) + direction;
    const int count = static_cast<int>(PerformanceArpDirection::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    (void)setArpDirection(static_cast<PerformanceArpDirection>(next));
}
bool PerformanceKeyboard::setArpDirection(PerformanceArpDirection direction) {
    if (direction >= PerformanceArpDirection::Count) return false;
    if (pendingClocked_.arpDirection == direction) return true;
    pendingClocked_.arpDirection = direction;
    stageClockedConfig();
    return true;
}
const char* PerformanceKeyboard::arpDirectionName() const {
    switch (pendingClocked_.arpDirection) {
        case PerformanceArpDirection::Up: return "UP";
        case PerformanceArpDirection::Down: return "DOWN";
        case PerformanceArpDirection::UpDown: return "UPDN";
        case PerformanceArpDirection::DownUp: return "DNUP";
        case PerformanceArpDirection::AsPlayed: return "PLAYED";
        case PerformanceArpDirection::Random: return "RANDOM";
        case PerformanceArpDirection::Count: break;
    }
    return "UP";
}
void PerformanceKeyboard::setArpRate(PerformanceRate rate) {
    if (rate >= PerformanceRate::Count || pendingClocked_.rate == rate) return;
    pendingClocked_.rate = rate;
    stageClockedConfig();
}
void PerformanceKeyboard::cycleArpRate(int direction) {
    int next = static_cast<int>(pendingClocked_.rate) + direction;
    const int count = static_cast<int>(PerformanceRate::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    setArpRate(static_cast<PerformanceRate>(next));
}
void PerformanceKeyboard::setGatePercent(uint8_t percent) {
    percent = clampPercent(percent);
    if (percent < 5) percent = 5;
    if (pendingClocked_.gatePercent == percent) return;
    pendingClocked_.gatePercent = percent;
    stageClockedConfig();
}
void PerformanceKeyboard::cycleGate(int direction) {
    int current = 0;
    int bestDistance = 256;
    for (int i = 0; i < static_cast<int>(sizeof(kGateOptions)); ++i) {
        const int distance = std::abs(static_cast<int>(pendingClocked_.gatePercent) - kGateOptions[i]);
        if (distance < bestDistance) { bestDistance = distance; current = i; }
    }
    int next = current + (direction >= 0 ? 1 : -1);
    const int count = static_cast<int>(sizeof(kGateOptions));
    while (next < 0) next += count;
    while (next >= count) next -= count;
    setGatePercent(kGateOptions[next]);
}
void PerformanceKeyboard::setArpOctaves(uint8_t octaves) {
    octaves = static_cast<uint8_t>(std::max<int>(1, std::min<int>(4, octaves)));
    if (pendingClocked_.arpOctaves == octaves) return;
    pendingClocked_.arpOctaves = octaves;
    stageClockedConfig();
}
void PerformanceKeyboard::cycleArpOctaves(int direction) {
    int next = pendingClocked_.arpOctaves + (direction >= 0 ? 1 : -1);
    if (next < 1) next = 4;
    if (next > 4) next = 1;
    setArpOctaves(static_cast<uint8_t>(next));
}
void PerformanceKeyboard::setLatchEnabled(bool enabled) {
    if (enabled && !pendingClocked_.arpEnabled) return;
    if (pendingClocked_.latchEnabled == enabled) return;
    if (enabled) captureLatchNow();
    pendingClocked_.latchEnabled = enabled;
    stageClockedConfig();
}

void PerformanceKeyboard::cycleStrum(int direction) {
    if (pendingClocked_.arpEnabled) return;
    int current = 0;
    for (int i = 0; i < static_cast<int>(sizeof(kStrumOptionsMs)); ++i) {
        if (kStrumOptionsMs[i] == strumMs_) { current = i; break; }
    }
    int next = current + (direction >= 0 ? 1 : -1);
    const int count = static_cast<int>(sizeof(kStrumOptionsMs));
    while (next < 0) next += count;
    while (next >= count) next -= count;
    strumMs_ = kStrumOptionsMs[next];
    if (strumMs_ > 0 && pendingClocked_.ratchetCount > 1) {
        pendingClocked_.ratchetCount = 1;
        stageClockedConfig();
    }
}
void PerformanceKeyboard::cycleStrumDirection(int direction) {
    int next = static_cast<int>(strumDirection_) + direction;
    const int count = static_cast<int>(PerformanceStrumDirection::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    strumDirection_ = static_cast<PerformanceStrumDirection>(next);
}
const char* PerformanceKeyboard::strumDirectionName() const {
    switch (strumDirection_) {
        case PerformanceStrumDirection::LowToHigh: return "LOW>HIGH";
        case PerformanceStrumDirection::HighToLow: return "HIGH>LOW";
        case PerformanceStrumDirection::AsPlayed: return "PLAYED";
        case PerformanceStrumDirection::Count: break;
    }
    return "LOW>HIGH";
}

void PerformanceKeyboard::cycleRatchet(int direction) {
    int next = pendingClocked_.ratchetCount + (direction >= 0 ? 1 : -1);
    if (next < 1) next = 4;
    if (next > 4) next = 1;
    pendingClocked_.ratchetCount = static_cast<uint8_t>(next);
    if (next > 1) strumMs_ = 0;
    stageClockedConfig();
}
void PerformanceKeyboard::setEuclideanLength(uint8_t length) {
    length = static_cast<uint8_t>(std::max<int>(1, std::min<int>(kEuclideanSteps, length)));
    pendingClocked_.euclideanLength = length;
    if (pendingClocked_.euclideanPulses > length) pendingClocked_.euclideanPulses = length;
    if (pendingClocked_.euclideanRotation >= length) pendingClocked_.euclideanRotation %= length;
    stageClockedConfig();
}
void PerformanceKeyboard::cycleEuclideanLength(int direction) {
    int next = pendingClocked_.euclideanLength + (direction >= 0 ? 1 : -1);
    if (next < 1) next = kEuclideanSteps;
    if (next > kEuclideanSteps) next = 1;
    setEuclideanLength(static_cast<uint8_t>(next));
}
void PerformanceKeyboard::cycleEuclideanPulses(int direction) {
    int next = pendingClocked_.euclideanPulses + (direction >= 0 ? 1 : -1);
    const int max = pendingClocked_.euclideanLength;
    if (next < 0) next = max;
    if (next > max) next = 0;
    pendingClocked_.euclideanPulses = static_cast<uint8_t>(next);
    stageClockedConfig();
}
void PerformanceKeyboard::rotateEuclidean(int direction) {
    const int length = pendingClocked_.euclideanLength;
    int next = pendingClocked_.euclideanRotation + (direction >= 0 ? 1 : -1);
    while (next < 0) next += length;
    while (next >= length) next -= length;
    pendingClocked_.euclideanRotation = static_cast<uint8_t>(next);
    stageClockedConfig();
}

void PerformanceKeyboard::setMutationSeed(uint32_t seed) {
    pendingClocked_.mutation.seed = seed == 0 ? 0x47505631u : seed;
    stageClockedConfig();
}
void PerformanceKeyboard::setMutationProbability(uint8_t skipPercent,
                                                 uint8_t octaveJumpPercent,
                                                 uint8_t deviatePercent) {
    pendingClocked_.mutation.skipPercent = clampPercent(skipPercent);
    pendingClocked_.mutation.octaveJumpPercent = clampPercent(octaveJumpPercent);
    pendingClocked_.mutation.deviatePercent = clampPercent(deviatePercent);
    stageClockedConfig();
}

int PerformanceKeyboard::activeNote() const {
    return heldCount_ == 0 ? -1 : held_[heldCount_ - 1].note;
}
