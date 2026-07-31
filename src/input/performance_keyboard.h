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

class PerformanceKeyboard {
public:
    static constexpr std::size_t kMaxHeldNotes = 19;
    // Live performance uses a wider range than the original two-octave keyboard.
    // C0..B6 safely covers the -2..+2 octave shifts for every supported scale
    // while remaining inside normal 7-bit MIDI note values.
    static constexpr uint8_t kMinNote = 12;
    static constexpr uint8_t kMaxNote = 95;
    static constexpr uint8_t kRootC2 = 36;
    static constexpr int8_t kMinOctaveShift = -2;
    static constexpr int8_t kMaxOctaveShift = 2;
    static constexpr uint8_t kSeqtrakDrumNote = 60;
    static constexpr uint8_t kSeqtrakDrumChannelCount = 7;

    explicit PerformanceKeyboard(MusicalEventRouter& router)
        : router_(router) {}

    // Returns true when the key belongs to NOTE mode and must not fall through
    // to legacy shortcuts. A transport-blocked performance key is consumed but
    // does not emit a NoteOn.
    bool keyDown(char physicalKey, uint8_t velocity = 100);
    bool keyUp(char physicalKey);

    // Reconciles the held-note stack against the physical keyboard matrix.
    // This recovers from a missed key-up without applying an arbitrary timeout.
    void releaseMissingKeys(const char* pressedKeys, std::size_t pressedCount);

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    void setNoteModeEnabled(bool enabled);
    void toggleNoteMode() { setNoteModeEnabled(!noteModeEnabled_); }
    bool noteModeEnabled() const { return noteModeEnabled_; }

    // PatternPlayer owns the internal synth voices while transport is running.
    // Starting transport clears live notes and disables new performance events
    // until it stops. The selected target remains unchanged.
    void setTransportPlaying(bool playing);
    bool transportPlaying() const { return transportPlaying_; }
    bool liveInputAllowed() const {
        return enabled_ && noteModeEnabled_ && !transportPlaying_;
    }

    void setTarget(MusicalEventTarget target);
    MusicalEventTarget target() const { return target_; }
    void cycleTarget(int direction);
    const char* targetName() const;
    // Returns the single MIDI channel for melodic targets. Drums returns the
    // first channel in its native CH1..7 range for legacy callers/UI helpers.
    uint8_t targetMidiChannel() const;

    void panic();

    void setScale(PerformanceScale scale);
    PerformanceScale scale() const { return scale_; }
    void cycleScale(int direction);
    const char* scaleName() const;

    bool shiftOctave(int direction);
    int8_t octaveShift() const { return octaveShift_; }

    int activeNote() const;
    std::size_t heldCount() const { return heldCount_; }

    bool noteForKey(char physicalKey, uint8_t& note) const;
    static bool isPerformanceKey(char physicalKey);
    static bool scaleDegreeForKey(char physicalKey, uint8_t& degree);

private:
    struct HeldNote {
        char physicalKey{0};
        uint8_t note{0};
        uint8_t velocity{0};
        uint8_t channel{0};
    };

    static char normalizeKey(char key);
    static bool isUpperRowKey(char key);
    static bool containsKey(const char* keys, std::size_t count, char key);
    static uint8_t intervalForDegree(PerformanceScale scale, uint8_t degree);
    static bool drumChannelForKey(char physicalKey, uint8_t& zeroBasedChannel);

    int findHeld(char physicalKey) const;
    void emitNoteOn(const HeldNote& held);
    void emitNoteOff(uint8_t note, uint8_t channel = 0);
    void emitAllNotesOff();

    MusicalEventRouter& router_;
    HeldNote held_[kMaxHeldNotes]{};
    std::size_t heldCount_{0};
    PerformanceScale scale_{PerformanceScale::NaturalMinor};
    MusicalEventTarget target_{MusicalEventTarget::SynthA};
    int8_t octaveShift_{0};
    bool enabled_{true};
    bool noteModeEnabled_{true};
    bool transportPlaying_{false};
};
