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
    static constexpr uint8_t kMinNote = 24;
    static constexpr uint8_t kMaxNote = 71;
    static constexpr uint8_t kRootC2 = 36;
    // The lower manual starts at C2 and the upper manual one octave higher.
    // One safe downward shift keeps every supported scale/key inside 24..71;
    // octave-up returns to the default position.
    static constexpr int8_t kMinOctaveShift = -1;
    static constexpr int8_t kMaxOctaveShift = 0;

    explicit PerformanceKeyboard(MusicalEventRouter& router)
        : router_(router) {}

    bool keyDown(char physicalKey, uint8_t velocity = 100);
    bool keyUp(char physicalKey);

    // Reconciles the held-note stack against the physical keyboard matrix.
    // This recovers from a missed key-up without applying an arbitrary timeout.
    void releaseMissingKeys(const char* pressedKeys, std::size_t pressedCount);

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    // PatternPlayer owns Synth A while transport is running. Starting transport
    // clears live notes and disables new performance events until it stops.
    void setTransportPlaying(bool playing);
    bool transportPlaying() const { return transportPlaying_; }
    bool liveInputAllowed() const { return enabled_ && !transportPlaying_; }

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
    static bool scaleDegreeForKey(char physicalKey, uint8_t& degree);

private:
    struct HeldNote {
        char physicalKey{0};
        uint8_t note{0};
        uint8_t velocity{0};
    };

    static char normalizeKey(char key);
    static bool isUpperRowKey(char key);
    static bool containsKey(const char* keys, std::size_t count, char key);
    static uint8_t intervalForDegree(PerformanceScale scale, uint8_t degree);

    int findHeld(char physicalKey) const;
    void emitNoteOn(const HeldNote& held);
    void emitNoteOff(uint8_t note);
    void emitAllNotesOff();

    MusicalEventRouter& router_;
    HeldNote held_[kMaxHeldNotes]{};
    std::size_t heldCount_{0};
    PerformanceScale scale_{PerformanceScale::NaturalMinor};
    int8_t octaveShift_{0};
    bool enabled_{true};
    bool transportPlaying_{false};
};
