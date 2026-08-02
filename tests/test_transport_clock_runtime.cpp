#include <cassert>
#include <cmath>
#include <cstdint>

#include "src/midi/transport_clock_runtime.h"

using namespace GroovePuterMidi;

namespace {
uint32_t g_callbackCount = 0;
TransportClockSource g_callbackSource = TransportClockSource::GroovePuterInternal;
bool g_callbackFollow = true;

void onControlChanged(TransportClockSource source, bool followEnabled) {
    ++g_callbackCount;
    g_callbackSource = source;
    g_callbackFollow = followEnabled;
}
}  // namespace

int main() {
    TransportClockRuntime runtime;
    assert(runtime.source() == TransportClockSource::GroovePuterInternal);
    assert(runtime.externalFollowEnabled());

    // Restoring persisted state bypasses the persistence callback so boot does
    // not rewrite NVS simply because a schema-v1/v2 record was loaded.
    runtime.applyPersistedControl(TransportClockSource::SeqtrakExternal, false);
    assert(runtime.source() == TransportClockSource::SeqtrakExternal);
    assert(!runtime.externalFollowEnabled());
    assert(g_callbackCount == 0);

    runtime.setControlChangedCallback(&onControlChanged);
    runtime.setSource(TransportClockSource::SeqtrakExternal);
    runtime.setExternalFollowEnabled(false);
    assert(g_callbackCount == 0);

    runtime.setExternalFollowEnabled(true);
    assert(runtime.externalFollowEnabled());
    assert(g_callbackCount == 1);
    assert(g_callbackSource == TransportClockSource::SeqtrakExternal);
    assert(g_callbackFollow);

    assert(runtime.toggleSource() == TransportClockSource::GroovePuterInternal);
    assert(runtime.source() == TransportClockSource::GroovePuterInternal);
    assert(g_callbackCount == 2);
    assert(g_callbackSource == TransportClockSource::GroovePuterInternal);
    assert(g_callbackFollow);

    assert(!runtime.toggleExternalFollowEnabled());
    assert(!runtime.externalFollowEnabled());
    assert(g_callbackCount == 3);
    assert(g_callbackSource == TransportClockSource::GroovePuterInternal);
    assert(!g_callbackFollow);

    assert(runtime.toggleSource() == TransportClockSource::SeqtrakExternal);
    assert(runtime.source() == TransportClockSource::SeqtrakExternal);
    assert(g_callbackCount == 4);
    assert(g_callbackSource == TransportClockSource::SeqtrakExternal);
    assert(!g_callbackFollow);

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
    assert(!snapshot.externalFollowEnabled);
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
