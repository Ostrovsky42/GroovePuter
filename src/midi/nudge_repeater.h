#pragma once
#ifndef GROOVEPUTER_MIDI_NUDGE_REPEATER_H
#define GROOVEPUTER_MIDI_NUDGE_REPEATER_H

#include <cstdint>

namespace GroovePuterMidi {

// Auto-repeat for a held external pitch button, with the same feel as a held arrow key on the
// Cardputer: the first step happens on the press (the caller does it), then after kDelayMs one step
// every kIntervalMs until the button is released. Times are milliseconds (wrap-around safe).
class NudgeRepeater {
public:
    static constexpr uint32_t kDelayMs = 350;
    static constexpr uint32_t kIntervalMs = 80;

    void onPress(int direction, uint32_t nowMs) {
        direction_ = direction < 0 ? -1 : (direction > 0 ? 1 : 0);
        nextMs_ = nowMs + kDelayMs;
    }
    void onRelease() { direction_ = 0; }
    void reset() { direction_ = 0; }

    // The direction of a repeat step that is due now (-1 / +1), or 0.
    int poll(uint32_t nowMs) {
        if (direction_ == 0 || static_cast<int32_t>(nowMs - nextMs_) < 0) return 0;
        nextMs_ = nowMs + kIntervalMs;
        return direction_;
    }

    bool active() const { return direction_ != 0; }

private:
    int direction_{0};
    uint32_t nextMs_{0};
};

}  // namespace GroovePuterMidi

#endif
