#pragma once

#include <cstdint>

#include "external_midi_clock_tracker.h"
#include "external_midi_transport_event_queue.h"
#include "transport_clock_source.h"

namespace GroovePuterMidi {

enum class ExternalTransportCommand : uint8_t {
    None = 0,
    Start,
    Continue,
    Stop,
};

struct ExternalClockBlockResult {
    ExternalTransportCommand command{ExternalTransportCommand::None};
    ExternalClockEstimate estimate{};
    bool sourceChanged{false};
    bool queueFailure{false};
};

class ExternalMidiClockFollower {
public:
    ExternalClockBlockResult processBlock(
            ExternalMidiTransportEventQueue& queue,
            TransportClockSource source,
            uint32_t nowMicros) {
        ExternalClockBlockResult result{};
        if (!haveSource_ || source != source_) {
            source_ = source;
            haveSource_ = true;
            tracker_.reset();
            queue.clearFailure();
            result.sourceChanged = true;
            result.command = ExternalTransportCommand::Stop;
            if (source != TransportClockSource::SeqtrakExternal) {
                queue.discardPending();
            }
        }

        if (source != TransportClockSource::SeqtrakExternal) {
            queue.discardPending();
            queue.clearFailure();
            result.estimate = tracker_.estimate(nowMicros);
            return result;
        }

        if (queue.failed()) {
            queue.discardPending();
            queue.clearFailure();
            tracker_.onFailure(nowMicros);
            ++failureCount_;
            result.command = ExternalTransportCommand::Stop;
            result.queueFailure = true;
            result.estimate = tracker_.estimate(nowMicros);
            return result;
        }

        ExternalMidiTransportEvent event{};
        while (queue.tryPop(event)) {
            switch (event.type) {
                case ExternalMidiTransportEventType::Clock:
                    tracker_.onClock(event.timestampMicros,
                                     event.pulseOrdinal);
                    break;
                case ExternalMidiTransportEventType::Start:
                    tracker_.onStart(event.timestampMicros);
                    result.command = ExternalTransportCommand::Start;
                    break;
                case ExternalMidiTransportEventType::Continue:
                    tracker_.onContinue(event.timestampMicros);
                    result.command = ExternalTransportCommand::Continue;
                    break;
                case ExternalMidiTransportEventType::Stop:
                    tracker_.onStop(event.timestampMicros);
                    result.command = ExternalTransportCommand::Stop;
                    break;
            }
        }

        const bool wasRunning = tracker_.transportRunning();
        tracker_.update(nowMicros);
        if (wasRunning && !tracker_.transportRunning() &&
            tracker_.state() == ExternalClockLockState::Lost) {
            result.command = ExternalTransportCommand::Stop;
        }
        result.estimate = tracker_.estimate(nowMicros);
        return result;
    }

    uint32_t failureCount() const { return failureCount_; }
    const ExternalMidiClockTracker& tracker() const { return tracker_; }

private:
    ExternalMidiClockTracker tracker_;
    TransportClockSource source_{TransportClockSource::GroovePuterInternal};
    uint32_t failureCount_{0};
    bool haveSource_{false};
};

}  // namespace GroovePuterMidi
