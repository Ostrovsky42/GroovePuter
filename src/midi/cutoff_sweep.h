#pragma once
#ifndef GROOVEPUTER_CUTOFF_SWEEP_H
#define GROOVEPUTER_CUTOFF_SWEEP_H

#include <cmath>
#include <cstdint>

#include "midi_realtime_word.h"

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
    double startBar() const { return startBar_; }

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

// Transport facts the automation needs, copied from the project timeline snapshot.
struct CutoffTransportView {
    bool valid{false};
    bool playing{false};
    uint32_t epoch{0};   // changes on every Start, restart or Continue
    double bar{0.0};    // absolute bar position of the project
};

// Drives a CutoffSweep from musical time. Stop cancels the sweep and keeps the
// held value; a new epoch (Start or Continue) cancels it too, so nothing resumes
// or restarts by itself. The dispatcher calls tick() once per pass.
class CutoffAutomation {
public:
    // Manual edit: cancels any sweep and makes the value known. The value is
    // delivered by the dispatcher's level check, not by this return value.
    bool manual(uint8_t value, uint8_t& send) {
        known_ = true;
        return sweep_.setManual(value, send);
    }

    // The receiver's value is known only after the user has set one: a fresh
    // device never gets an unsolicited CC74.
    bool known() const { return known_; }

    // Starts a sweep from the held value. Only while the transport plays and the
    // value is known; a sweep is never armed for a later Start.
    bool startSweep(uint8_t target, const CutoffTransportView& t) {
        if (!known_ || !t.valid || !t.playing) return false;
        sweep_.begin(target, t.bar);
        epoch_ = t.epoch;
        epochKnown_ = true;
        return true;
    }

    void cancelSweep() { sweep_.cancel(); }

    // Returns true and sets `send` when a CC74 value must go out this pass.
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
    // Whole bars of the sweep that have passed at `nowBar`, 0..kDurationBars. Read
    // only: it never changes the sweep and is zero when no sweep is running.
    uint8_t barsElapsed(double nowBar) const {
        if (!sweep_.active()) return 0;
        double elapsed = nowBar - sweep_.startBar();
        if (elapsed < 0.0) elapsed = 0.0;
        if (elapsed >= static_cast<double>(CutoffSweep::kDurationBars)) {
            return CutoffSweep::kDurationBars;
        }
        return static_cast<uint8_t>(elapsed);
    }
    uint8_t value() const { return sweep_.value(); }

private:
    CutoffSweep sweep_;
    uint32_t epoch_{0};
    bool epochKnown_{false};
    bool known_{false};
};

// UI-to-dispatcher request word. The UI publishes one request; only the dispatcher
// reads it. Layout: bits 0..6 value, bits 7..8 op, bits 9..31 sequence (non-zero).
struct CutoffRequest {
    enum Op : uint8_t { kNone = 0, kManual = 1, kStartSweep = 2, kCancelSweep = 3 };

    static uint32_t encode(Op op, uint8_t value, uint32_t sequence) {
        const uint32_t seq = (sequence & 0x7FFFFFu) == 0u ? 1u : (sequence & 0x7FFFFFu);
        return (seq << 9u) | (static_cast<uint32_t>(op) << 7u) | (value & 0x7Fu);
    }
    static Op op(uint32_t word) { return static_cast<Op>((word >> 7u) & 0x3u); }
    static uint8_t value(uint32_t word) { return static_cast<uint8_t>(word & 0x7Fu); }
    static uint32_t sequence(uint32_t word) { return word >> 9u; }
};

// Shared, portable channel between the UI and the MIDI dispatcher for CUTOFF.
// The UI is the only writer of requests; the dispatcher is the only writer of the
// published value. Both sides see the same words on the device and in SDL.
class CutoffControlChannel {
public:
    CutoffControlChannel() = default;

    // UI task only.
    void request(CutoffRequest::Op op, uint8_t value) {
        ++sequence_;
        request_.storeRelease(CutoffRequest::encode(op, value, sequence_));
    }

    // Dispatcher only.
    uint32_t loadRequest() const { return request_.loadAcquire(); }
    // Layout: bits 0..6 value, bit 7 sweeping, bits 8..10 whole bars elapsed,
    // bit 11 known (the user has set a value on this voice).
    void publish(uint8_t value, bool sweeping, uint8_t barsElapsed, bool known) {
        published_.storeRelease(static_cast<uint32_t>(value & 0x7Fu) |
                                (sweeping ? 0x80u : 0u) |
                                (static_cast<uint32_t>(barsElapsed & 0x7u) << 8u) |
                                (known ? 0x800u : 0u));
    }

    // Any task.
    uint8_t value() const { return static_cast<uint8_t>(published_.loadAcquire() & 0x7Fu); }
    bool sweeping() const { return (published_.loadAcquire() & 0x80u) != 0u; }
    uint8_t barsElapsed() const { return static_cast<uint8_t>((published_.loadAcquire() >> 8u) & 0x7u); }
    bool known() const { return (published_.loadAcquire() & 0x800u) != 0u; }

private:
    MidiRealtimeWord request_;
    MidiRealtimeWord published_;
    uint32_t sequence_{0};
};

// One channel per synth voice: index 0 is Synth A, 1 is Synth B.
inline CutoffControlChannel gCutoffControl[2];

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_CUTOFF_SWEEP_H
