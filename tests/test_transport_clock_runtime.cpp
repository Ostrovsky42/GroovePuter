#include <cassert>
#include <cmath>

#include "src/midi/transport_clock_runtime.h"

using namespace GroovePuterMidi;

int main() {
    TransportClockRuntime runtime;
    assert(runtime.source() == TransportClockSource::GroovePuterInternal);
    assert(runtime.toggleSource() == TransportClockSource::SeqtrakExternal);
    assert(runtime.source() == TransportClockSource::SeqtrakExternal);

    ExternalClockEstimate estimate{};
    estimate.state = ExternalClockLockState::Locked;
    estimate.transportRunning = true;
    estimate.validTempo = true;
    estimate.bpmQ16 = 123u << 16;
    estimate.transportEpoch = 9;
    estimate.pulseCount = 456;
    runtime.publishExternalEstimate(estimate, 3);

    TransportClockRuntimeSnapshot snapshot{};
    assert(runtime.trySnapshot(snapshot));
    assert(snapshot.source == TransportClockSource::SeqtrakExternal);
    assert(snapshot.externalState == ExternalClockLockState::Locked);
    assert(snapshot.externalRunning);
    assert(snapshot.externalTempoValid);
    assert(std::fabs(snapshot.externalBpm() - 123.0) < 1.0e-9);
    assert(snapshot.externalEpoch == 9);
    assert(snapshot.externalPulseCount == 456);
    assert(snapshot.externalFailureCount == 3);
    return 0;
}
