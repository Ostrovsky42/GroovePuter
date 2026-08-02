#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include "external_midi_clock_tracker.h"
#include "midi_realtime_word.h"
#include "transport_clock_source.h"

namespace GroovePuterMidi {

struct TransportClockRuntimeSnapshot {
    TransportClockSource source{TransportClockSource::GroovePuterInternal};
    ExternalClockLockState externalState{ExternalClockLockState::Waiting};
    bool externalFollowEnabled{true};
    bool externalRunning{false};
    bool externalTempoValid{false};
    // The displayed BPM is the actual SEQTRAK source tempo. The small local PLL
    // trim is exposed separately as a phase correction diagnostic.
    uint32_t externalBpmQ16{0};
    int32_t externalPhaseErrorQ16{0};
    int32_t externalPhaseCorrectionQ16{0};
    uint32_t externalEpoch{0};
    uint32_t externalPulseCount{0};
    uint32_t externalFailureCount{0};

    double externalBpm() const {
        return static_cast<double>(externalBpmQ16) / 65536.0;
    }

    double externalPhaseErrorSteps() const {
        return static_cast<double>(externalPhaseErrorQ16) / 65536.0;
    }

    double externalPhaseCorrectionSteps() const {
        return static_cast<double>(externalPhaseCorrectionQ16) / 65536.0;
    }
};

using TransportClockControlChangedCallback = void (*)(
    TransportClockSource source,
    bool externalFollowEnabled);

class TransportClockRuntime {
public:
    TransportClockSource source() const {
        return normalizeTransportClockSource(
            static_cast<uint8_t>(source_.loadAcquire()));
    }

    void setSource(TransportClockSource source) {
        const TransportClockSource normalized = normalizeTransportClockSource(
            static_cast<uint8_t>(source));
        if (normalized == this->source()) return;
        source_.storeRelease(static_cast<uint32_t>(normalized));
        notifyControlChanged();
    }

    TransportClockSource toggleSource() {
        const TransportClockSource next =
            source() == TransportClockSource::GroovePuterInternal
                ? TransportClockSource::SeqtrakExternal
                : TransportClockSource::GroovePuterInternal;
        setSource(next);
        return next;
    }

    // Zero-initialized runtime state intentionally means FOLLOW ON. This keeps
    // the common path usable without static initialization order dependencies.
    bool externalFollowEnabled() const {
        return externalFollowDisabled_.loadAcquire() == 0u;
    }

    void setExternalFollowEnabled(bool enabled) {
        if (enabled == externalFollowEnabled()) return;
        externalFollowDisabled_.storeRelease(enabled ? 0u : 1u);
        notifyControlChanged();
    }

    bool toggleExternalFollowEnabled() {
        const bool enabled = !externalFollowEnabled();
        setExternalFollowEnabled(enabled);
        return enabled;
    }

    // Applies decoded settings before registering the persistence callback.
    // This avoids rewriting NVS merely because a record was loaded or migrated.
    void applyPersistedControl(TransportClockSource source,
                               bool externalFollowEnabled) {
        source_.storeRelease(static_cast<uint32_t>(
            normalizeTransportClockSource(static_cast<uint8_t>(source))));
        externalFollowDisabled_.storeRelease(
            externalFollowEnabled ? 0u : 1u);
    }

    void setControlChangedCallback(
            TransportClockControlChangedCallback callback) {
        controlChangedCallback_ = callback;
    }

    void publishExternalEstimate(const ExternalClockEstimate& estimate,
                                 uint32_t failureCount) {
        const uint32_t version = version_.loadRelaxed();
        version_.storeRelease(version + 1u);
        externalState_.storeRelaxed(static_cast<uint32_t>(estimate.state));
        externalRunning_.storeRelaxed(estimate.transportRunning ? 1u : 0u);
        externalTempoValid_.storeRelaxed(estimate.validTempo ? 1u : 0u);
        externalBpmQ16_.storeRelaxed(
            estimate.sourceBpmQ16 != 0 ? estimate.sourceBpmQ16
                                       : estimate.bpmQ16);
        externalPhaseErrorQ16_.storeRelaxed(
            static_cast<uint32_t>(toSignedQ16(estimate.phaseErrorSteps)));
        externalPhaseCorrectionQ16_.storeRelaxed(
            static_cast<uint32_t>(toSignedQ16(
                estimate.phaseCorrectionSteps)));
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
            out.externalFollowEnabled = externalFollowEnabled();
            out.externalState = static_cast<ExternalClockLockState>(
                externalState_.loadRelaxed());
            out.externalRunning = externalRunning_.loadRelaxed() != 0u;
            out.externalTempoValid =
                externalTempoValid_.loadRelaxed() != 0u;
            out.externalBpmQ16 = externalBpmQ16_.loadRelaxed();
            out.externalPhaseErrorQ16 = static_cast<int32_t>(
                externalPhaseErrorQ16_.loadRelaxed());
            out.externalPhaseCorrectionQ16 = static_cast<int32_t>(
                externalPhaseCorrectionQ16_.loadRelaxed());
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
        if (!trySnapshot(out)) {
            out.source = source();
            out.externalFollowEnabled = externalFollowEnabled();
        }
        return out;
    }

private:
    static int32_t toSignedQ16(double value) {
        const double scaled = value * 65536.0;
        if (scaled >= static_cast<double>(
                std::numeric_limits<int32_t>::max())) {
            return std::numeric_limits<int32_t>::max();
        }
        if (scaled <= static_cast<double>(
                std::numeric_limits<int32_t>::min())) {
            return std::numeric_limits<int32_t>::min();
        }
        return static_cast<int32_t>(std::llround(scaled));
    }

    void notifyControlChanged() {
        if (controlChangedCallback_ != nullptr) {
            controlChangedCallback_(source(), externalFollowEnabled());
        }
    }

    MidiRealtimeWord source_;
    MidiRealtimeWord externalFollowDisabled_;
    MidiRealtimeWord version_;
    MidiRealtimeWord externalState_;
    MidiRealtimeWord externalRunning_;
    MidiRealtimeWord externalTempoValid_;
    MidiRealtimeWord externalBpmQ16_;
    MidiRealtimeWord externalPhaseErrorQ16_;
    MidiRealtimeWord externalPhaseCorrectionQ16_;
    MidiRealtimeWord externalEpoch_;
    MidiRealtimeWord externalPulseCount_;
    MidiRealtimeWord externalFailureCount_;
    TransportClockControlChangedCallback controlChangedCallback_{nullptr};
};

inline TransportClockRuntime& transportClockRuntime() {
    static TransportClockRuntime runtime;
    return runtime;
}

}  // namespace GroovePuterMidi
