#pragma once

#include <cstdint>

#include "external_midi_clock_tracker.h"
#include "midi_realtime_word.h"
#include "transport_clock_source.h"

namespace GroovePuterMidi {

struct TransportClockRuntimeSnapshot {
    TransportClockSource source{TransportClockSource::GroovePuterInternal};
    ExternalClockLockState externalState{ExternalClockLockState::Waiting};
    bool externalRunning{false};
    bool externalTempoValid{false};
    uint32_t externalBpmQ16{0};
    uint32_t externalEpoch{0};
    uint32_t externalPulseCount{0};
    uint32_t externalFailureCount{0};

    double externalBpm() const {
        return static_cast<double>(externalBpmQ16) / 65536.0;
    }
};

class TransportClockRuntime {
public:
    TransportClockSource source() const {
        return normalizeTransportClockSource(
            static_cast<uint8_t>(source_.loadAcquire()));
    }

    void setSource(TransportClockSource source) {
        source_.storeRelease(static_cast<uint32_t>(source));
    }

    TransportClockSource toggleSource() {
        const TransportClockSource next =
            source() == TransportClockSource::GroovePuterInternal
                ? TransportClockSource::SeqtrakExternal
                : TransportClockSource::GroovePuterInternal;
        setSource(next);
        return next;
    }

    void publishExternalEstimate(const ExternalClockEstimate& estimate,
                                 uint32_t failureCount) {
        const uint32_t version = version_.loadRelaxed();
        version_.storeRelease(version + 1u);
        externalState_.storeRelaxed(static_cast<uint32_t>(estimate.state));
        externalRunning_.storeRelaxed(estimate.transportRunning ? 1u : 0u);
        externalTempoValid_.storeRelaxed(estimate.validTempo ? 1u : 0u);
        externalBpmQ16_.storeRelaxed(estimate.bpmQ16);
        externalEpoch_.storeRelaxed(estimate.transportEpoch);
        externalPulseCount_.storeRelaxed(
            static_cast<uint32_t>(estimate.pulseCount));
        externalFailureCount_.storeRelaxed(failureCount);
        version_.storeRelease(version + 2u);
    }

    bool trySnapshot(TransportClockRuntimeSnapshot& out) const {
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t before = version_.loadAcquire();
            if (before & 1u) continue;
            out.source = source();
            out.externalState = static_cast<ExternalClockLockState>(
                externalState_.loadRelaxed());
            out.externalRunning = externalRunning_.loadRelaxed() != 0u;
            out.externalTempoValid =
                externalTempoValid_.loadRelaxed() != 0u;
            out.externalBpmQ16 = externalBpmQ16_.loadRelaxed();
            out.externalEpoch = externalEpoch_.loadRelaxed();
            out.externalPulseCount = externalPulseCount_.loadRelaxed();
            out.externalFailureCount = externalFailureCount_.loadRelaxed();
            const uint32_t after = version_.loadAcquire();
            if (before == after && !(after & 1u)) return true;
        }
        return false;
    }

    TransportClockRuntimeSnapshot snapshot() const {
        TransportClockRuntimeSnapshot out{};
        if (!trySnapshot(out)) out.source = source();
        return out;
    }

private:
    MidiRealtimeWord source_;
    MidiRealtimeWord version_;
    MidiRealtimeWord externalState_;
    MidiRealtimeWord externalRunning_;
    MidiRealtimeWord externalTempoValid_;
    MidiRealtimeWord externalBpmQ16_;
    MidiRealtimeWord externalEpoch_;
    MidiRealtimeWord externalPulseCount_;
    MidiRealtimeWord externalFailureCount_;
};

inline TransportClockRuntime& transportClockRuntime() {
    static TransportClockRuntime runtime;
    return runtime;
}

}  // namespace GroovePuterMidi
