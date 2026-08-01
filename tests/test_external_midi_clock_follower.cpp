#include <cassert>
#include <cmath>
#include <cstdint>

#include "src/midi/external_midi_clock_follower.h"

using namespace GroovePuterMidi;

namespace {
void pushClock(ExternalMidiTransportEventQueue& queue,
               uint32_t& now,
               uint32_t& ordinal) {
    now += 20833;
    assert(queue.tryPushClock(now, ++ordinal));
}
}

int main() {
    ExternalMidiTransportEventQueue queue;
    ExternalMidiClockFollower follower;
    uint32_t now = 1000000;
    uint32_t ordinal = 0;

    assert(queue.tryPushClock(now, ++ordinal));
    auto result = follower.processBlock(
        queue, TransportClockSource::GroovePuterInternal, now);
    assert(result.sourceChanged);
    assert(queue.approximateSize() == 0);

    // A Start arriving immediately after the UI source switch must survive the
    // AudioTask's first source-change block.
    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Start, now, ordinal));
    for (int i = 0; i < 7; ++i) pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.sourceChanged);
    assert(result.command == ExternalTransportCommand::Start);
    assert(result.estimate.transportRunning);
    assert(result.estimate.state == ExternalClockLockState::Locked);
    assert(std::fabs(
        static_cast<double>(result.estimate.bpmQ16) / 65536.0 - 120.0) < 0.1);

    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Stop, now, ordinal));
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.command == ExternalTransportCommand::Stop);
    const double stoppedAt = result.estimate.absoluteProjectSteps;
    pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(std::fabs(result.estimate.absoluteProjectSteps - stoppedAt) < 0.001);

    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Continue, now, ordinal));
    pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.command == ExternalTransportCommand::Continue);
    assert(result.estimate.transportRunning);
    assert(result.estimate.absoluteProjectSteps > stoppedAt);

    ExternalMidiTransportEventQueue failedQueue;
    for (std::size_t i = 0;
         i < ExternalMidiTransportEventQueue::kCapacity;
         ++i) {
        assert(failedQueue.tryPushCritical(
            ExternalMidiTransportEventType::Start,
            now + static_cast<uint32_t>(i), ordinal));
    }
    assert(!failedQueue.tryPushCritical(
        ExternalMidiTransportEventType::Stop, now, ordinal));
    result = follower.processBlock(
        failedQueue, TransportClockSource::SeqtrakExternal, now);
    assert(result.queueFailure);
    assert(result.command == ExternalTransportCommand::Stop);
    assert(result.estimate.state == ExternalClockLockState::Lost);
    assert(follower.failureCount() == 1);
    assert(failedQueue.approximateSize() == 0);

    return 0;
}
