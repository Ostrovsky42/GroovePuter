#pragma once
#ifndef GROOVEPUTER_CUTOFF_SWEEP_H
#define GROOVEPUTER_CUTOFF_SWEEP_H

#include <cmath>
#include <cstdint>

#include "midi_realtime_word.h"

namespace GroovePuterMidi {

// Zero-based MIDI channels of the two synth performance routes. They must match
// the UsbMidiRouteConfig fields used by the dispatcher (performanceSynthAChannel,
// performanceSynthBChannel); a host test checks that they agree.
inline constexpr uint8_t kSynthAChannel = 7;   // MIDI channel 8, SEQTRAK SYNTH 1
inline constexpr uint8_t kSynthBChannel = 8;   // MIDI channel 9, SEQTRAK SYNTH 2

// Controllers the synth tab may send. Each is a documented Yamaha SEQTRAK synth
// control on channels 1..11. Excluded on purpose: CC5, CC65 and CC26 (portamento
// and receiver mode, owned by the Pattern slide path), CC27..29 (the arpeggiator
// makes its own notes), CC116..119 (DX only, channel 10) and CC102..115 (assigned
// effect parameters on the master channels).
inline constexpr uint8_t kControllerCount = 8;
struct CutoffController {
    uint8_t number;
    const char* name;
    uint8_t minValue;  // Yamaha's usable range; PAN is 1..127
};
inline constexpr CutoffController kControllers[kControllerCount] = {
    {74, "CUTOFF", 0},  {71, "RESO", 0},   {73, "ATTACK", 0}, {75, "RELEASE", 0},
    {91, "REVERB", 0},  {94, "DELAY", 0},  {7, "VOLUME", 0},  {10, "PAN", 1},
};

// Sweep lengths the user can choose, in bars. Musically they divide a phrase evenly.
inline constexpr uint8_t kDurationCount = 4;
inline constexpr uint8_t kDurations[kDurationCount] = {1, 2, 4, 8};
inline constexpr uint8_t kDefaultDurationIndex = 2;   // four bars

// The value a controller can carry on the wire: clamped to its usable range. The
// dispatcher and the output both use it, so a delivered check compares like with like.
inline uint8_t clampForController(int index, uint8_t value) {
    if (index < 0) return value;
    const uint8_t minimum = kControllers[index].minValue;
    if (value < minimum) return minimum;
    return value > 127 ? 127 : value;
}

// Index of a controller number in kControllers, or -1 when it is not allowed.
inline int controllerIndex(uint8_t number) {
    for (uint8_t i = 0; i < kControllerCount; ++i) {
        if (kControllers[i].number == number) return i;
    }
    return -1;
}

// Single-controller sweep over a fixed musical span. Pure state machine: it takes
// the project's bar position as input and never reads a timer. Two shapes:
// RAMP goes from the held value to the target over the span; RETURN goes there
// over the first half and back to the start over the second half.
class CutoffSweep {
public:
    static constexpr uint8_t kMaxValue = 127;

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

    // Sets the held value without a sweep (used when another controller is selected).
    void restore(uint8_t value) {
        cancel();
        value_ = clamp(value);
        sentOnce_ = true;
    }

    // Starts a sweep from the held value to `target`. The start value is captured
    // here, so the ramp begins where the user left it.
    void begin(uint8_t target, double startBar, bool returning, uint8_t bars = 4) {
        startValue_ = value_;
        target_ = clamp(target);
        startBar_ = startBar;
        returning_ = returning;
        bars_ = bars == 0 ? 4 : bars;
        active_ = true;
    }

    // Called on stop or on a disconnect. The held value stays where it is.
    void cancel() { active_ = false; }

    bool active() const { return active_; }
    bool returning() const { return returning_; }
    uint8_t value() const { return value_; }
    double startBar() const { return startBar_; }
    uint8_t bars() const { return bars_; }

    // Called with the current bar position. Returns true and sets `send` when the
    // rounded value changed since the last value. Finishing sends the end value
    // once, then the sweep is inactive and never sends again.
    bool poll(double nowBar, uint8_t& send) {
        if (!active_) return false;
        double elapsed = nowBar - startBar_;
        if (elapsed < 0.0) elapsed = 0.0;
        const double span = static_cast<double>(bars_);
        const double from = static_cast<double>(startValue_);
        const double to = static_cast<double>(target_);
        uint8_t next = value_;
        if (elapsed >= span) {
            next = returning_ ? startValue_ : target_;
            active_ = false;
        } else if (returning_) {
            const double half = span / 2.0;
            if (elapsed < half) {
                next = ramp(from, to, elapsed / half);
            } else {
                next = ramp(to, from, (elapsed - half) / half);
            }
        } else {
            next = ramp(from, to, elapsed / span);
        }
        if (next == value_ && sentOnce_) return false;
        value_ = next;
        sentOnce_ = true;
        send = next;
        return true;
    }

private:
    static uint8_t clamp(uint8_t v) { return v > kMaxValue ? kMaxValue : v; }

    // Linear step from a to b at ratio r, rounded toward a so the far end is only
    // reached when the leg completes, never a fraction of a bar early.
    static uint8_t ramp(double a, double b, double r) {
        const double raw = a + (b - a) * r;
        const double rounded = b >= a ? std::floor(raw) : std::ceil(raw);
        return clamp(static_cast<uint8_t>(rounded < 0.0 ? 0.0 : rounded));
    }

    uint8_t value_{64};
    uint8_t startValue_{64};
    uint8_t target_{110};
    double startBar_{0.0};
    bool active_{false};
    bool returning_{false};
    bool sentOnce_{false};
    uint8_t bars_{4};
};

// Transport facts the automation needs, copied from the project timeline snapshot.
struct CutoffTransportView {
    bool valid{false};
    bool playing{false};
    uint32_t epoch{0};   // changes on every Start, restart or Continue
    double bar{0.0};    // absolute bar position of the project
};

// Drives a CutoffSweep from musical time for the selected controller. Stop cancels
// the sweep and keeps the held value; a new epoch (Start or Continue) cancels it
// too, so nothing resumes or restarts by itself.
class CutoffAutomation {
public:
    // Manual edit: cancels any sweep and makes the value known. The value is
    // delivered by the dispatcher's level check, not by this return value.
    bool manual(uint8_t value, uint8_t& send) {
        known_ = true;
        return sweep_.setManual(value, send);
    }

    // Selects a controller's held value without sending it. `known` is that
    // controller's own knowledge; an unknown controller stays silent until set.
    void restore(uint8_t value, bool known) {
        sweep_.restore(value);
        known_ = known;
        epochKnown_ = false;
    }

    // The receiver's value is known only after the user has set one: a fresh
    // device never gets an unsolicited CC.
    bool known() const { return known_; }

    // Starts a sweep from the held value. Only while the transport plays and the
    // value is known; a sweep is never armed for a later Start.
    bool startSweep(uint8_t target, const CutoffTransportView& t, bool returning,
                    uint8_t bars = 4) {
        if (!known_ || !t.valid || !t.playing) return false;
        sweep_.begin(target, t.bar, returning, bars);
        epoch_ = t.epoch;
        epochKnown_ = true;
        return true;
    }

    void cancelSweep() { sweep_.cancel(); }

    // Returns true and sets `send` when the held value changed on this pass.
    bool tick(const CutoffTransportView& t, uint8_t& send) {
        if (!t.valid || !t.playing) {
            sweep_.cancel();
            epochKnown_ = false;
            return false;
        }
        if (epochKnown_ && t.epoch != epoch_) {
            sweep_.cancel();           // a restart or Continue never resumes a sweep
            epochKnown_ = false;
            return false;
        }
        return sweep_.poll(t.bar, send);
    }

    bool sweepActive() const { return sweep_.active(); }
    bool returning() const { return sweep_.returning(); }
    uint8_t value() const { return sweep_.value(); }

    // Whole bars of the sweep that have passed at `nowBar`, 0..its length. Read
    // only: zero when no sweep is running.
    uint8_t barsElapsed(double nowBar) const {
        if (!sweep_.active()) return 0;
        double elapsed = nowBar - sweep_.startBar();
        if (elapsed < 0.0) elapsed = 0.0;
        if (elapsed >= static_cast<double>(sweep_.bars())) return sweep_.bars();
        return static_cast<uint8_t>(elapsed);
    }

private:
    CutoffSweep sweep_;
    uint32_t epoch_{0};
    bool epochKnown_{false};
    bool known_{false};
};

// UI-to-dispatcher request word. Only the UI writes requests; only the dispatcher
// reads them. Layout: bits 0..6 value, bits 7..9 op, bits 10..12 controller index,
// bits 13..31 sequence (non-zero, so repeated requests are distinct).
struct CutoffRequest {
    enum Op : uint8_t {
        kNone = 0,
        kManual = 1,
        kStartSweep = 2,
        kCancelSweep = 3,
        kSelectController = 4,
        kSetShape = 5,     // value 0 = RAMP, 1 = RETURN
        kSetDuration = 6,  // value = index into kDurations
    };

    static uint32_t encode(Op op, uint8_t value, uint32_t sequence, uint8_t controller = 0) {
        const uint32_t seq = (sequence & 0x7FFFFu) == 0u ? 1u : (sequence & 0x7FFFFu);
        return (seq << 13u) | ((static_cast<uint32_t>(controller) & 0x7u) << 10u) |
               ((static_cast<uint32_t>(op) & 0x7u) << 7u) | (value & 0x7Fu);
    }
    static Op op(uint32_t word) { return static_cast<Op>((word >> 7u) & 0x7u); }
    static uint8_t value(uint32_t word) { return static_cast<uint8_t>(word & 0x7Fu); }
    static uint8_t controller(uint32_t word) { return static_cast<uint8_t>((word >> 10u) & 0x7u); }
    static uint32_t sequence(uint32_t word) { return word >> 13u; }
};

// Shared, portable channel between the UI and the MIDI dispatcher for one synth.
// The UI is the only writer of requests; the dispatcher is the only writer of the
// published state. The same words on the device and in SDL.
class CutoffControlChannel {
public:
    // UI task only.
    void request(CutoffRequest::Op op, uint8_t value, uint8_t controller = 0) {
        ++sequence_;
        request_.storeRelease(CutoffRequest::encode(op, value, sequence_, controller));
    }

    // Dispatcher only.
    uint32_t loadRequest() const { return request_.loadAcquire(); }

    // Layout: bits 0..6 value, bit 7 sweeping, bits 8..11 bars elapsed (0..8),
    // bit 12 known, bits 13..15 selected controller index, bit 16 RETURN shape,
    // bits 17..18 duration index.
    void publish(uint8_t value, bool sweeping, uint8_t barsElapsed, bool known,
                 uint8_t controller, bool returning, uint8_t durationIndex = kDefaultDurationIndex) {
        published_.storeRelease(static_cast<uint32_t>(value & 0x7Fu) |
                                (sweeping ? 0x80u : 0u) |
                                (static_cast<uint32_t>(barsElapsed & 0xFu) << 8u) |
                                (known ? 0x1000u : 0u) |
                                (static_cast<uint32_t>(controller & 0x7u) << 13u) |
                                (returning ? 0x10000u : 0u) |
                                (static_cast<uint32_t>(durationIndex & 0x3u) << 17u));
    }

    // Any task.
    uint8_t value() const { return static_cast<uint8_t>(published_.loadAcquire() & 0x7Fu); }
    bool sweeping() const { return (published_.loadAcquire() & 0x80u) != 0u; }
    uint8_t barsElapsed() const { return static_cast<uint8_t>((published_.loadAcquire() >> 8u) & 0xFu); }
    bool known() const { return (published_.loadAcquire() & 0x1000u) != 0u; }
    uint8_t controller() const { return static_cast<uint8_t>((published_.loadAcquire() >> 13u) & 0x7u); }
    bool returning() const { return (published_.loadAcquire() & 0x10000u) != 0u; }
    uint8_t durationIndex() const { return static_cast<uint8_t>((published_.loadAcquire() >> 17u) & 0x3u); }

private:
    MidiRealtimeWord request_;
    MidiRealtimeWord published_;
    uint32_t sequence_{0};
};

// One channel per synth voice: index 0 is Synth A, 1 is Synth B.
inline CutoffControlChannel gCutoffControl[2];

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_CUTOFF_SWEEP_H
