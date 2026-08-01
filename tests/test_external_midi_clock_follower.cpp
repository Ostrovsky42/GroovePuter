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

bool closeEnough(double a, double b, double epsilon) {
    return std::fabs(a - b) <= epsilon;
}

double q16Bpm(uint32_t value) {
    return static_cast<double>(value) / 65536.0;
}
}

int main() {
    projectTransportTimeline().resetPublisher();

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
    assert(closeEnough(q16Bpm(result.estimate.sourceBpmQ16), 120.0, 0.1));
    assert(closeEnough(q16Bpm(result.estimate.bpmQ16),
                       q16Bpm(result.estimate.sourceBpmQ16),
                       0.05));
    assert(closeEnough(result.estimate.phaseCorrectionSteps, 0.0, 1.0e-12));

    // The previous audio block is behind the external timeline. A bounded PLL
    // trim must speed the local sequencer up without changing the source BPM
    // displayed to the user or seeking the phase directly.
    projectTransportTimeline().publishBlock(
        10, 512, 0.0f, 120.0f, 22050.0f, true);
    pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.command == ExternalTransportCommand::None);
    assert(result.estimate.phaseErrorSteps > 0.0);
    assert(result.estimate.phaseCorrectionSteps > 0.0);
    assert(result.estimate.phaseCorrectionSteps <=
           ExternalMidiClockFollower::kMaximumCorrectionSteps + 1.0e-12);
    assert(result.estimate.bpmQ16 > result.estimate.sourceBpmQ16);
    const uint32_t positiveTrimBpmQ16 = result.estimate.bpmQ16;

    // Repeated blocks inside one hysteresis region keep the exact same drive
    // BPM. Normal phase convergence must not create a tempo revision per block.
    projectTransportTimeline().publishBlock(
        11, 512, 0.1f, 120.0f, 22050.0f, true);
    pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.estimate.phaseErrorSteps > 0.0);
    assert(result.estimate.bpmQ16 == positiveTrimBpmQ16);

    // If the local sequencer is ahead, the bounded trim changes sign.
    projectTransportTimeline().publishBlock(
        12, 512, 3.0f, 120.0f, 22050.0f, true);
    pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.estimate.phaseErrorSteps < 0.0);
    assert(result.estimate.phaseCorrectionSteps < 0.0);
    assert(result.estimate.bpmQ16 < result.estimate.sourceBpmQ16);

    // Three F8 packets drained with one receive timestamp represent three real
    // musical pulses. The follower coalesces the run, advances half a project
    // step and keeps the source tempo stable instead of measuring 0 us gaps.
    const double beforeBuffered = result.estimate.absoluteProjectSteps;
    now += 3u * 20833u;
    for (int i = 0; i < 3; ++i) {
        assert(queue.tryPushClock(now, ++ordinal));
    }
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(closeEnough(result.estimate.absoluteProjectSteps,
                       beforeBuffered + 0.5,
                       0.001));
    assert(closeEnough(q16Bpm(result.estimate.sourceBpmQ16), 120.0, 0.2));

    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Stop, now, ordinal));
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(result.command == ExternalTransportCommand::Stop);
    assert(closeEnough(result.estimate.phaseCorrectionSteps, 0.0, 1.0e-12));
    const double stoppedAt = result.estimate.absoluteProjectSteps;
    pushClock(queue, now, ordinal);
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now);
    assert(std::fabs(result.estimate.absoluteProjectSteps - stoppedAt) < 0.001);
    assert(closeEnough(result.estimate.phaseCorrectionSteps, 0.0, 1.0e-12));

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
