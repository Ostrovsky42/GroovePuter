#pragma once

#include <cstddef>
#include <cstdint>

#include "musical_event_router.h"

enum class PerformanceScale : uint8_t {
    Chromatic = 0,
    Major,
    NaturalMinor,
    MinorPentatonic,
    Dorian,
    Count,
};

enum class PerformanceChordMode : uint8_t {
    Off = 0,
    Major,
    Minor,
    Fifth,
    Minor7,
    Memory,
    Count,
};

enum class PerformanceArpDirection : uint8_t {
    Up = 0,
    Down,
    UpDown,
    Count,
};

enum class PerformanceVoiceMode : uint8_t {
    Mono = 0,
    Poly,
    Count,
};

class PerformanceKeyboard {
public:
    static constexpr std::size_t kMaxHeldNotes = 19;
    static constexpr uint8_t kMinNote = 12;
    static constexpr uint8_t kMaxNote = 95;
    static constexpr uint8_t kRootC2 = 36;
    static constexpr int8_t kMinOctaveShift = -2;
    static constexpr int8_t kMaxOctaveShift = 2;
    static constexpr uint8_t kSeqtrakDrumNote = 60;
    static constexpr uint8_t kSeqtrakDrumChannelCount = 7;
    static constexpr uint8_t kEuclideanSteps = 16;

    explicit PerformanceKeyboard(MusicalEventRouter& router)
        : router_(router) {}

    bool keyDown(char physicalKey, uint8_t velocity = 100);
    bool keyUp(char physicalKey);
    void releaseMissingKeys(const char* pressedKeys, std::size_t pressedCount);

    // Called automatically from the existing per-loop transport heartbeat on
    // Cardputer. Host tests call this directly with deterministic timestamps.
    void service(uint32_t nowMicros);
    void setTempoBpm(float bpm);
    float tempoBpm() const { return tempoBpm_; }

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    void setNoteModeEnabled(bool enabled);
    void toggleNoteMode() { setNoteModeEnabled(!noteModeEnabled_); }
    bool noteModeEnabled() const { return noteModeEnabled_; }

    void setTransportPlaying(bool playing);
    bool transportPlaying() const { return transportPlaying_; }
    bool liveInputAllowed() const {
        return enabled_ && noteModeEnabled_;
    }

    void setTarget(MusicalEventTarget target);
    MusicalEventTarget target() const { return target_; }
    void cycleTarget(int direction);
    const char* targetName() const;
    uint8_t targetMidiChannel() const;

    void setVoiceMode(PerformanceVoiceMode mode);
    void toggleVoiceMode();
    PerformanceVoiceMode voiceMode() const { return voiceMode_; }
    const char* voiceModeName() const;
    // Direct POLY mode is manual external MIDI polyphony. Drums already own
    // independent lanes, while ARP/Chord/Strum/Ratchet/Euclidean keep their
    // existing generated-note ownership model.
    bool directPolyphonyEnabled() const;

    void panic();

    void setScale(PerformanceScale scale);
    PerformanceScale scale() const { return scale_; }
    void cycleScale(int direction);
    const char* scaleName() const;

    bool shiftOctave(int direction);
    int8_t octaveShift() const { return octaveShift_; }

    void setChordMode(PerformanceChordMode mode);
    PerformanceChordMode chordMode() const { return chordMode_; }
    void cycleChordMode(int direction = 1);
    const char* chordModeName() const;

    bool captureChordMemory();
    void clearChordMemory();
    std::size_t chordMemorySize() const { return chordMemoryCount_; }

    void setArpeggiatorEnabled(bool enabled);
    void toggleArpeggiator() { setArpeggiatorEnabled(!arpeggiatorEnabled_); }
    bool arpeggiatorEnabled() const { return arpeggiatorEnabled_; }
    void cycleArpDirection(int direction = 1);
    PerformanceArpDirection arpDirection() const { return arpDirection_; }
    const char* arpDirectionName() const;

    void cycleStrum(int direction = 1);
    uint8_t strumMs() const { return strumMs_; }

    void cycleRatchet(int direction = 1);
    uint8_t ratchetCount() const { return ratchetCount_; }

    void cycleEuclideanPulses(int direction = 1);
    uint8_t euclideanPulses() const { return euclideanPulses_; }
    void rotateEuclidean(int direction);
    uint8_t euclideanRotation() const { return euclideanRotation_; }

    bool transformedPlaybackEnabled() const;

    int activeNote() const;
    int activeVelocity() const {
        return heldCount_ > 0 ? static_cast<int>(held_[heldCount_ - 1].velocity) : -1;
    }
    std::size_t heldCount() const { return heldCount_; }

    bool isPhysicalKeyHeld(char physicalKey) const {
        return findHeld(normalizeKey(physicalKey)) >= 0;
    }
    bool isPitchClassHeld(uint8_t pitchClass) const {
        pitchClass %= 12;
        for (std::size_t i = 0; i < heldCount_; ++i) {
            if ((held_[i].note % 12) == pitchClass) return true;
        }
        return false;
    }

    bool noteForKey(char physicalKey, uint8_t& note) const;
    static bool isPerformanceKey(char physicalKey);
    static bool scaleDegreeForKey(char physicalKey, uint8_t& degree);

private:
    static constexpr std::size_t kMaxGeneratedNotes = 16;
    // One maximum-density step can contain 8 chord notes * 4 ratchets *
    // NoteOn/NoteOff = 64 events. Keep headroom for the second half of the
    // current step while the following transport step is prepared.
    static constexpr std::size_t kMaxScheduledEvents = 112;
    static constexpr std::size_t kMaxChordMemoryNotes = 8;

    struct HeldNote {
        char physicalKey{0};
        uint8_t note{0};
        uint8_t velocity{0};
        uint8_t channel{0};
    };

    struct ScheduledEvent {
        bool active{false};
        uint32_t dueMicros{0};
        MusicalEvent event{};
    };

    static char normalizeKey(char key);
    static bool isUpperRowKey(char key);
    static bool containsKey(const char* keys, std::size_t count, char key);
    static uint8_t intervalForDegree(PerformanceScale scale, uint8_t degree);
    static bool drumChannelForKey(char physicalKey, uint8_t& zeroBasedChannel);
    static bool due(uint32_t nowMicros, uint32_t dueMicros);

    int findHeld(char physicalKey) const;
    void emitNoteOn(const HeldNote& held);
    void emitNoteOff(uint8_t note, uint8_t channel = 0);
    void emitPolyNoteOn(const HeldNote& held);
    void emitPolyNoteOff(uint8_t note);
    void emitAllNotesOff();
    void routeGenerated(MusicalEventType type,
                        uint8_t note,
                        uint8_t velocity,
                        uint8_t channel = 0);

    void serviceHardwareClock();
    void processScheduled(uint32_t nowMicros);
    bool scheduleGenerated(MusicalEventType type,
                           uint8_t note,
                           uint8_t velocity,
                           uint32_t dueMicros,
                           uint8_t channel = 0);
    void clearScheduled();
    void stopGeneratedOutput();
    void rememberGeneratedOn(uint8_t note);
    void forgetGenerated(uint8_t note);

    bool stepEngineEnabled() const;
    uint32_t stepDurationMicros() const;
    bool serviceTransportStepClock(uint32_t nowMicros);
    bool euclideanStepActive(uint8_t step) const;
    void resetStepClock();
    void emitPerformanceStep(uint32_t stepStartMicros, uint32_t stepMicros);

    std::size_t buildChord(uint8_t baseNote,
                           uint8_t* notes,
                           std::size_t capacity) const;
    std::size_t buildArpPool(uint8_t* notes, std::size_t capacity) const;
    uint8_t selectArpNote(const uint8_t* notes, std::size_t count);
    void triggerDirectTransformed(uint32_t nowMicros);
    void restartAfterConfigurationChange();

    MusicalEventRouter& router_;
    HeldNote held_[kMaxHeldNotes]{};
    std::size_t heldCount_{0};
    PerformanceScale scale_{PerformanceScale::NaturalMinor};
    PerformanceChordMode chordMode_{PerformanceChordMode::Off};
    PerformanceArpDirection arpDirection_{PerformanceArpDirection::Up};
    PerformanceVoiceMode voiceMode_{PerformanceVoiceMode::Mono};
    MusicalEventTarget target_{MusicalEventTarget::SynthA};
    int8_t octaveShift_{0};
    bool enabled_{true};
    bool noteModeEnabled_{true};
    bool transportPlaying_{false};

    uint8_t chordMemoryIntervals_[kMaxChordMemoryNotes]{};
    std::size_t chordMemoryCount_{0};

    bool arpeggiatorEnabled_{false};
    uint8_t strumMs_{0};
    uint8_t ratchetCount_{1};
    uint8_t euclideanPulses_{0};
    uint8_t euclideanRotation_{0};
    float tempoBpm_{120.0f};

    ScheduledEvent scheduled_[kMaxScheduledEvents]{};
    uint8_t generatedNotes_[kMaxGeneratedNotes]{};
    std::size_t generatedNoteCount_{0};

    bool stepClockRunning_{false};
    uint32_t nextStepMicros_{0};
    uint32_t lastServiceMicros_{0};
    uint8_t euclideanStep_{0};
    std::size_t arpIndex_{0};
    bool arpAscending_{true};

    bool transportStepClockRunning_{false};
    uint32_t transportStepEpoch_{0};
    uint64_t transportStepOrdinal_{0};
    bool transportStepScheduled_{false};
    bool transportBlockAnchorValid_{false};
    uint32_t transportAnchorBlockSequence_{0};
    uint32_t transportAnchorMicros_{0};
};
