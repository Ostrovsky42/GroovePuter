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
    estimate.sourceBpmQ16 = 123u << 16;
    estimate.bpmQ16 = 126u << 16;  // local PLL drive tempo, not UI tempo
    estimate.phaseErrorSteps = -0.375;
    estimate.phaseCorrectionSteps = 0.0078125;
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
    assert(std::fabs(snapshot.externalPhaseErrorSteps() + 0.375) < 1.0e-9);
    assert(std::fabs(snapshot.externalPhaseCorrectionSteps() - 0.0078125) <
           1.0e-9);
    assert(snapshot.externalEpoch == 9);
    assert(snapshot.externalPulseCount == 456);
    assert(snapshot.externalFailureCount == 3);

    // Trackers without a distinct source field remain backward-compatible.
    estimate.sourceBpmQ16 = 0;
    estimate.bpmQ16 = 95u << 16;
    estimate.phaseErrorSteps = 0.0;
    estimate.phaseCorrectionSteps = 0.0;
    runtime.publishExternalEstimate(estimate, 4);
    assert(runtime.trySnapshot(snapshot));
    assert(std::fabs(snapshot.externalBpm() - 95.0) < 1.0e-9);
    assert(snapshot.externalFailureCount == 4);
    return 0;
}
