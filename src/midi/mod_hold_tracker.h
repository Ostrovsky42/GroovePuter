#pragma once
#ifndef GROOVEPUTER_MIDI_MOD_HOLD_TRACKER_H
#define GROOVEPUTER_MIDI_MOD_HOLD_TRACKER_H

#include <cstdint>

namespace GroovePuterMidi {

// Decides tap or hold for the external Mod button. A tap (released before kHoldMs) is acted on at the
// release; a hold that lasts kHoldMs fires once while still down, and its release then does nothing.
// Times are milliseconds from any monotonic clock (unsigned wrap-around safe).
class ModHoldTracker {
public:
    static constexpr uint32_t kHoldMs = 800;

    void onPress(uint32_t nowMs) {
        held_ = true;
        longFired_ = false;
        pressedMs_ = nowMs;
    }

    // True when the release ends a tap (act now); false for a hold that already fired or no press.
    bool onRelease() {
        const bool tap = held_ && !longFired_;
        held_ = false;
        return tap;
    }

    // Call regularly. True exactly once per press, when the hold time is reached.
    bool pollLongHold(uint32_t nowMs) {
        if (!held_ || longFired_ || static_cast<uint32_t>(nowMs - pressedMs_) < kHoldMs) return false;
        longFired_ = true;
        return true;
    }

    // Session ended, a release was lost, ...: forget the press without acting on it.
    void reset() { held_ = false; }

    bool held() const { return held_; }

private:
    bool held_{false};
    bool longFired_{false};
    uint32_t pressedMs_{0};
};

}  // namespace GroovePuterMidi

#endif
