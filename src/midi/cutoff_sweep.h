#pragma once
#ifndef GROOVEPUTER_CUTOFF_SWEEP_H
#define GROOVEPUTER_CUTOFF_SWEEP_H

#include <cmath>
#include <cstdint>

namespace GroovePuterMidi {

// CC74 (filter cutoff) sweep over a fixed musical span. Pure state machine: it
// takes the project's bar position as an input and never reads a timer, so the
// result depends only on musical time and the BPM-independent bar count.
//
// Ownership: the sweep owns only the value it sends. Manual changes and stop
// cancel it; a cancelled or finished sweep never sends again on its own.
class CutoffSweep {
public:
    static constexpr uint8_t kController = 74;
    static constexpr uint8_t kMaxValue = 127;
    static constexpr uint8_t kDurationBars = 4;

    // Manual change: sets the held value and cancels any sweep. Returns the value
    // the caller should send, or false when it is unchanged.
    bool setManual(uint8_t value, uint8_t& send) {
        cancel();
        value = clamp(value);
        if (value == value_ && sentOnce_) return false;
        value_ = value;
        sentOnce_ = true;
        send = value;
        return true;
    }

    // Called on stop or on a disconnect. The held value stays where it is.
    void cancel() { active_ = false; }

    bool active() const { return active_; }
    uint8_t value() const { return value_; }

    // Called with the current bar position. Returns true and sets `send` when the
    // rounded value changed since the last value sent. Finishing sends the target
    // once, then the sweep is inactive and never sends again.
    bool poll(double nowBar, uint8_t& send) {
        if (!active_) return false;
        double elapsed = nowBar - startBar_;
        if (elapsed < 0.0) elapsed = 0.0;
        const double span = static_cast<double>(kDurationBars);
        uint8_t next = value_;
        if (elapsed >= span) {
            next = target_;
            active_ = false;
        } else {
            const double from = static_cast<double>(startValue_);
            const double to = static_cast<double>(target_);
            const double raw = from + (to - from) * (elapsed / span);
            // Round toward the start value, so the target is reached only when the
            // span completes, never a fraction of a bar early.
            const double rounded = to >= from ? std::floor(raw) : std::ceil(raw);
            next = clamp(static_cast<uint8_t>(rounded));
        }
        if (next == value_ && sentOnce_) return false;
        value_ = next;
        sentOnce_ = true;
        send = next;
        return true;
    }

    // Starts a sweep from the currently held value to `target` over kDurationBars.
    // The start value is captured here, so the ramp begins where the user left it.
    void begin(uint8_t target, double startBar) {
        startValue_ = value_;
        target_ = clamp(target);
        startBar_ = startBar;
        active_ = true;
    }

private:
    static uint8_t clamp(uint8_t v) { return v > kMaxValue ? kMaxValue : v; }

    uint8_t value_{64};
    uint8_t startValue_{64};
    uint8_t target_{110};
    double startBar_{0.0};
    bool active_{false};
    bool sentOnce_{false};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_CUTOFF_SWEEP_H
