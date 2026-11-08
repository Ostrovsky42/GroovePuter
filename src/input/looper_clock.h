#pragma once
#ifndef GROOVEPUTER_LOOPER_CLOCK_H
#define GROOVEPUTER_LOOPER_CLOCK_H

#include <cmath>
#include <cstdint>

#include "call_response_listener.h"  // TransportView, sixteenthMicrosFor (read only)

// LOOPER clock (phase 1). Logic copied from CallResponseListener
// (service/positionAt/microsAtStep), without its auto-stop and without
// touching any loop data: a clock change never erases the layer.
//   - Transport: project steps while PLAY runs, re-anchored on every new
//     audio block (the block sequence changes);
//   - Free: the PERFORM tempo, step 0 = the moment it was started; a tempo
//     change keeps the position continuous;
//   - None: no clock.
// Main loop only.
namespace GroovePuterLooper {

using GroovePuterCallResponse::TransportView;

class LoopClock {
public:
    enum class Source : uint8_t { None = 0, Transport, Free };
    enum class Event : uint8_t { None = 0, TransportStarted, TransportStopped };
    static constexpr int32_t kReanchorMicros = int32_t{1} << 28;  // ~268 s

    // transport == nullptr: the snapshot was not readable this pass; the
    // clock keeps its source and extrapolates.
    Event update(uint32_t now, const TransportView* transport, float performBpm) {
        Event event = Event::None;
        if (transport && transport->playing) {
            const uint16_t block = static_cast<uint16_t>(transport->blockSequence);
            if (source_ != Source::Transport || transport->epoch != transportEpoch_) {
                source_ = Source::Transport;
                transportEpoch_ = transport->epoch;
                lastBlock_ = block;
                anchorSteps_ = transport->steps;
                anchorMicros_ = now;
                event = Event::TransportStarted;
            } else if (block != lastBlock_) {
                lastBlock_ = block;
                anchorSteps_ = transport->steps;
                anchorMicros_ = now;
            }
            sixteenthMicros_ = sixteenthFor(transport->bpm);
            return event;
        }
        if (transport && source_ == Source::Transport) {
            stop();
            event = Event::TransportStopped;
        }
        const uint32_t sixteenth = sixteenthFor(performBpm);
        // Tempo change: continuous position. Also re-anchored every few
        // minutes so the elapsed time never leaves int32 (a loop may run for
        // hours; micros() itself wraps after 71 minutes).
        if (source_ == Source::Free &&
            (sixteenth != sixteenthMicros_ ||
             static_cast<int32_t>(now - anchorMicros_) > kReanchorMicros)) {
            anchorSteps_ = positionAt(now);
            anchorMicros_ = now;
        }
        if (source_ != Source::Transport) sixteenthMicros_ = sixteenth;
        return event;
    }

    // Free clock from now: step `atStep` at `now`, at the last PERFORM tempo.
    void startFree(uint32_t now, double atStep = 0.0) {
        source_ = Source::Free;
        anchorSteps_ = atStep;
        anchorMicros_ = now;
        if (sixteenthMicros_ == 0) sixteenthMicros_ = sixteenthFor(120.0);
    }
    void stop() {
        source_ = Source::None;
        anchorSteps_ = 0.0;
    }

    Source source() const { return source_; }
    bool running() const { return source_ != Source::None && sixteenthMicros_ != 0; }
    uint32_t sixteenthMicros() const { return sixteenthMicros_; }

    double positionAt(uint32_t now) const {
        if (!running()) return 0.0;
        const int32_t elapsed = static_cast<int32_t>(now - anchorMicros_);
        return anchorSteps_ + (elapsed > 0 ? static_cast<double>(elapsed) / sixteenthMicros_ : 0.0);
    }
    uint32_t microsAtStep(double step) const {
        const double offset = (step - anchorSteps_) * static_cast<double>(sixteenthMicros_);
        const int64_t rounded = static_cast<int64_t>(offset >= 0.0 ? offset + 0.5 : offset - 0.5);
        return anchorMicros_ + static_cast<uint32_t>(rounded);
    }

    static uint32_t sixteenthFor(double bpm) {
        return GroovePuterCallResponse::CallResponseListener::sixteenthMicrosFor(bpm);
    }

private:
    double anchorSteps_{0.0};
    uint32_t anchorMicros_{0};
    uint32_t sixteenthMicros_{0};
    uint32_t transportEpoch_{0};
    uint16_t lastBlock_{0};
    Source source_{Source::None};
};

}  // namespace GroovePuterLooper

#endif  // GROOVEPUTER_LOOPER_CLOCK_H
