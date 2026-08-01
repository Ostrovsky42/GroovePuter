#include <cassert>
#include <cstdint>

#include "src/midi/external_midi_clock_follower.h"

using namespace GroovePuterMidi;

int main() {
    ExternalMidiTransportEventQueue queue;
    ExternalMidiClockFollower follower;
    uint32_t now = 1000000;
    uint32_t ordinal = 0;

    follower.processBlock(
        queue, TransportClockSource::GroovePuterInternal, now, true);

    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Start, now, ordinal));
    for (int i = 0; i < 7; ++i) {
        now += 20833u;
        assert(queue.tryPushClock(now, ++ordinal));
    }
    auto result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now, true);
    assert(result.command == ExternalTransportCommand::Start);
    assert(result.estimate.transportRunning);

    // Turning Follow OFF is itself a local Stop at the next audio boundary.
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now, false);
    assert(result.followChanged);
    assert(result.command == ExternalTransportCommand::Stop);
    assert(!result.estimate.transportRunning);

    // Clock still measures while OFF, but FA/FB cannot start the local engine.
    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Start, now, ordinal));
    for (int i = 0; i < 7; ++i) {
        now += 20833u;
        assert(queue.tryPushClock(now, ++ordinal));
    }
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now, false);
    assert(result.command == ExternalTransportCommand::None);
    assert(!result.estimate.transportRunning);
    assert(result.estimate.validTempo);

    // Enabling Follow does not auto-start from an earlier ignored FA.
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now, true);
    assert(result.followChanged);
    assert(result.command == ExternalTransportCommand::None);
    assert(!result.estimate.transportRunning);

    // A new FB/FA after enabling is required.
    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Continue, now, ordinal));
    result = follower.processBlock(
        queue, TransportClockSource::SeqtrakExternal, now, true);
    assert(result.command == ExternalTransportCommand::Continue);
    assert(result.estimate.transportRunning);

    return 0;
}
