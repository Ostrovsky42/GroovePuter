#pragma once

#include <cstddef>
#include <cstdint>

#include "musical_event_router.h"
#include "performance_chord_detector.h"
#include "performance_instrument_types.h"
#include "performance_pulse.h"

class PerformanceKeyboard {
public:
    static constexpr std::size_t kMaxHeldNotes = 19;
    static constexpr std::size_t kMaxPolyChordNotes = 16;
    static constexpr std::size_t kMaxScheduledEvents = 112;
    static constexpr uint8_t kMinNote = 12;
    static constexpr uint8_t kMaxNote = 95;
    static constexpr uint8_t kRootC2 = 36;
    static constexpr int8_t kMinOctaveShift = -2;
    static constexpr int8_t kMaxOctaveShift = 2;
    static constexpr uint8_t kSeqtrakDrumNote = 60;
    static constexpr uint8_t kSeqtrakDrumChannelCount = 7;
    static constexpr uint8_t kEuclideanSteps = 16;
    static constexpr uint8_t kMinVelocity = 10;
    static constexpr uint8_t kMaxVelocity = 120;
    static constexpr uint8_t kVelocityStep = 10;
    static constexpr uint8_t kDefaultVelocity = 100;

    explicit PerformanceKeyboard(MusicalEventRouter& router)
        : router_(router) {
        pendingClocked_ = activeClocked_;
        mutationRng_.reset(activeClocked_.mutation.seed);
    }

    bool keyDown(char physicalKey, uint8_t velocity = 0);
    bool keyUp(char physicalKey);
    void releaseMissingKeys(const char* pressedKeys, std::size_t pressedCount);
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
    bool liveInputAllowed() const { return enabled_ && noteModeEnabled_; }

    void setTarget(MusicalEventTarget target);
    MusicalEventTarget target() const { return target_; }
    void cycleTarget(int direction);
    const char* targetName() const;
    uint8_t targetMidiChannel() const;

    void setVoiceMode(PerformanceVoiceMode mode);
    void toggleVoiceMode();
    PerformanceVoiceMode voiceMode() const { return voiceMode_; }
    const char* voiceModeName() const;
    bool directPolyphonyEnabled() const;

    void setVelocity(uint8_t velocity);
    bool adjustVelocity(int direction);
    uint8_t velocity() const { return keyVelocity_; }
    void panic();

    void setScale(PerformanceScale scale);
    PerformanceScale scale() const { return scale_; }
    void cycleScale(int direction);
    const char* scaleName() const;
    void setRootPitchClass(uint8_t pitchClass);
    void cycleRoot(int direction);
    uint8_t rootPitchClass() const { return rootPitchClass_; }
    const char* rootName() const;
    bool shiftOctave(int direction);
    int8_t octaveShift() const { return octaveShift_; }

    void setChordMode(PerformanceChordMode mode);
    PerformanceChordMode chordMode() const { return chordMode_; }
    void cycleChordMode(int direction = 1);
    const char* chordModeName() const;
    void setChordInversion(uint8_t inversion);
    void cycleChordInversion(int direction = 1);
    uint8_t chordInversion() const { return chordInversion_; }
    void setChordSpread(PerformanceSpread spread);
    void toggleChordSpread();
    PerformanceSpread chordSpread() const { return chordSpread_; }
    const char* chordSpreadName() const;
    void setVoiceLeading(PerformanceVoiceLeading mode);
    void toggleVoiceLeading();
    PerformanceVoiceLeading voiceLeading() const { return voiceLeading_; }
    const char* voiceLeadingName() const;
    void resetVoiceLeading();
    bool captureChordMemory();
    void clearChordMemory();
    std::size_t chordMemorySize() const { return chordMemoryCount_; }
    bool formatDetectedChord(char* out, std::size_t capacity) const;

    void setArpeggiatorEnabled(bool enabled);
    void toggleArpeggiator() { setArpeggiatorEnabled(!arpeggiatorEnabled()); }
    bool arpeggiatorEnabled() const { return pendingClocked_.arpEnabled; }
    void cycleArpDirection(int direction = 1);
    PerformanceArpDirection arpDirection() const { return pendingClocked_.arpDirection; }
    const char* arpDirectionName() const;
    void setArpRate(PerformanceRate rate);
    void cycleArpRate(int direction = 1);
    PerformanceRate arpRate() const { return pendingClocked_.rate; }
    const char* arpRateName() const { return performanceRateName(pendingClocked_.rate); }
    void setGatePercent(uint8_t percent);
    void cycleGate(int direction = 1);
    uint8_t gatePercent() const { return pendingClocked_.gatePercent; }
    void setArpOctaves(uint8_t octaves);
    void cycleArpOctaves(int direction = 1);
    uint8_t arpOctaves() const { return pendingClocked_.arpOctaves; }
    void setLatchEnabled(bool enabled);
    void toggleLatch() { setLatchEnabled(!latchEnabled()); }
    bool latchEnabled() const { return pendingClocked_.latchEnabled; }

    void cycleStrum(int direction = 1);
    uint8_t strumMs() const { return strumMs_; }
    void cycleStrumDirection(int direction = 1);
    PerformanceStrumDirection strumDirection() const { return strumDirection_; }
    const char* strumDirectionName() const;

    void cycleRatchet(int direction = 1);
    uint8_t ratchetCount() const { return pendingClocked_.ratchetCount; }
    void setEuclideanLength(uint8_t length);
    void cycleEuclideanLength(int direction = 1);
    uint8_t euclideanLength() const { return pendingClocked_.euclideanLength; }
    void cycleEuclideanPulses(int direction = 1);
    uint8_t euclideanPulses() const { return pendingClocked_.euclideanPulses; }
    void rotateEuclidean(int direction);
    uint8_t euclideanRotation() const { return pendingClocked_.euclideanRotation; }

    void setMutationSeed(uint32_t seed);
    void setMutationProbability(uint8_t skipPercent,
                                uint8_t octaveJumpPercent,
                                uint8_t deviatePercent);
    const PerformanceMutationConfig& mutationConfig() const {
        return pendingClocked_.mutation;
    }

    bool transformedPlaybackEnabled() const;
    int activeNote() const;
    int activeVelocity() const {
        return heldCount_ > 0 ? static_cast<int>(held_[heldCount_ - 1].velocity) : -1;
    }
    std::size_t heldCount() const { return heldCount_; }
    bool isPhysicalKeyHeld(char physicalKey) const {
        return findHeld(normalizeKey(physicalKey)) >= 0;
    }
    bool isPitchClassHeld(uint8_t pitchClass) const;
    bool noteForKey(char physicalKey, uint8_t& note) const;
    static bool isPerformanceKey(char physicalKey);
    static bool scaleDegreeForKey(char physicalKey, uint8_t& degree);

    std::size_t scheduledDepth() const;
    std::size_t scheduledHighWater() const { return scheduledHighWater_; }
    uint32_t scheduledOverflowCount() const { return scheduledOverflowCount_; }
    uint32_t staleGeneratedNoteOnDrops() const { return staleGeneratedNoteOnDrops_; }
    void resetPerformanceDiagnostics();
    uint64_t musicalPulseOrdinal() const { return musicalPulseOrdinal_; }
    uint64_t rateEpochOrdinal() const { return rateEpochOrdinal_; }
    uint32_t pulseEpoch() const { return pulseEpoch_; }
    PerformanceRate activeRate() const { return activeClocked_.rate; }

private:
    static constexpr std::size_t kMaxGeneratedNotes = kMaxPolyChordNotes;
    static constexpr std::size_t kMaxChordMemoryNotes = 8;
    static constexpr std::size_t kMaxLatchedNotes = kMaxHeldNotes;

    struct HeldNote {
        char physicalKey{0};
        uint8_t note{0};
        uint8_t velocity{0};
        uint8_t channel{0};
    };
    struct LatchedNote { uint8_t note{0}; uint8_t velocity{0}; };
    struct ScheduledEvent {
        bool active{false};
        uint32_t dueMicros{0};
        MusicalEvent event{};
    };

    static char normalizeKey(char key);
    static bool isUpperRowKey(char key);
    static bool containsKey(const char* keys, std::size_t count, char key);
    static bool drumChannelForKey(char physicalKey, uint8_t& zeroBasedChannel);
    static bool due(uint32_t nowMicros, uint32_t dueMicros);
    static uint8_t clampPercent(uint8_t value);

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

    bool activeStepEngineEnabled() const;
    bool requestedStepEngineEnabled() const;
    bool polyChordSustainEnabled() const;
    uint32_t pulseDurationMicros(PerformanceRate rate, double bpm) const;
    bool serviceTransportPulseClock(uint32_t nowMicros);
    bool euclideanPulseActive(uint64_t musicalOrdinal) const;
    void resetPulseClock(bool resetMusicalPhase);
    void stageClockedConfig();
    void commitPendingClockedConfig(double boundaryProjectStep,
                                    uint32_t boundaryMicros);
    void emitPerformancePulse(uint32_t pulseStartMicros,
                              uint32_t pulseMicros,
                              const PerformanceClockedConfig& pulseConfig);

    std::size_t buildChord(uint8_t baseNote,
                           uint8_t* notes,
                           std::size_t capacity,
                           bool applyVoiceLeading = true) const;
    std::size_t buildScaleChord(uint8_t baseNote,
                                bool seventh,
                                uint8_t* notes,
                                std::size_t capacity) const;
    std::size_t buildArpPool(uint8_t* notes,
                             uint8_t* velocities,
                             std::size_t capacity) const;
    uint8_t selectArpNote(const uint8_t* notes,
                          std::size_t count,
                          const PerformanceClockedConfig& pulseConfig);
    uint8_t velocityForArpNote(const uint8_t* notes,
                               const uint8_t* velocities,
                               std::size_t count,
                               uint8_t selected) const;
    void applyInversionAndSpread(uint8_t* notes, std::size_t count) const;
    void applyNearestVoiceLeading(uint8_t* notes, std::size_t count) const;
    void rememberVoicing(const uint8_t* notes, std::size_t count);
    void reconcileDirectPolyChord(uint32_t nowMicros);
    void revoiceDirectHarmony(uint32_t nowMicros);
    void triggerDirectTransformed(uint32_t nowMicros);
    void transitionPlaybackModeAtBoundary(bool oldStepEnabled,
                                          bool newStepEnabled,
                                          uint32_t boundaryMicros);

    void captureLatchNow();
    void commitLatchAtBoundary(const PerformanceClockedConfig& oldConfig,
                               const PerformanceClockedConfig& newConfig);
    std::size_t collectInputPool(LatchedNote* out, std::size_t capacity) const;

    MusicalEventRouter& router_;
    HeldNote held_[kMaxHeldNotes]{};
    std::size_t heldCount_{0};
    PerformanceScale scale_{PerformanceScale::NaturalMinor};
    PerformanceChordMode chordMode_{PerformanceChordMode::Off};
    PerformanceVoiceMode voiceMode_{PerformanceVoiceMode::Mono};
    MusicalEventTarget target_{MusicalEventTarget::SynthA};
    uint8_t rootPitchClass_{0};
    int8_t octaveShift_{0};
    uint8_t keyVelocity_{kDefaultVelocity};
    bool enabled_{true};
    bool noteModeEnabled_{true};
    bool transportPlaying_{false};

    uint8_t chordMemoryIntervals_[kMaxChordMemoryNotes]{};
    std::size_t chordMemoryCount_{0};
    uint8_t chordInversion_{0};
    PerformanceSpread chordSpread_{PerformanceSpread::Close};
    PerformanceVoiceLeading voiceLeading_{PerformanceVoiceLeading::Off};
    mutable uint8_t previousVoicing_[kMaxChordMemoryNotes]{};
    mutable std::size_t previousVoicingCount_{0};

    uint8_t strumMs_{0};
    PerformanceStrumDirection strumDirection_{PerformanceStrumDirection::LowToHigh};
    float tempoBpm_{120.0f};

    PerformanceClockedConfig activeClocked_{};
    PerformanceClockedConfig pendingClocked_{};
    bool clockedConfigPending_{false};
    PerformanceMutationRng mutationRng_{};
    uint32_t mutationSeedApplied_{0};

    LatchedNote latched_[kMaxLatchedNotes]{};
    std::size_t latchedCount_{0};
    LatchedNote pendingLatch_[kMaxLatchedNotes]{};
    std::size_t pendingLatchCount_{0};
    bool pendingLatchCapture_{false};
    bool latchReplaceArmed_{false};

    ScheduledEvent scheduled_[kMaxScheduledEvents]{};
    uint8_t generatedNotes_[kMaxGeneratedNotes]{};
    std::size_t generatedNoteCount_{0};
    std::size_t scheduledHighWater_{0};
    uint32_t scheduledOverflowCount_{0};
    uint32_t staleGeneratedNoteOnDrops_{0};

    bool standalonePulseRunning_{false};
    uint32_t nextStandalonePulseMicros_{0};
    uint32_t lastServiceMicros_{0};

    bool transportPulseClockRunning_{false};
    uint32_t transportPulseEpoch_{0};
    bool transportPulseScheduled_{false};
    bool transportBlockAnchorValid_{false};
    uint32_t transportAnchorBlockSequence_{0};
    uint32_t transportAnchorMicros_{0};
    double nextTransportPulseProjectStep_{0.0};
    double rateOriginProjectStep_{0.0};
    uint64_t rateEpochOrdinal_{0};
    uint64_t musicalPulseOrdinal_{0};
    uint32_t pulseEpoch_{0};

    std::size_t arpIndex_{0};
    bool arpAscending_{true};
};
