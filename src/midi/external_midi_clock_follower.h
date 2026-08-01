#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "external_midi_clock_tracker.h"
#include "external_midi_transport_event_queue.h"
#include "project_transport_timeline.h"
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
    // The PLL deliberately trims tempo instead of seeking the sequencer phase.
    // At 120 BPM / 512 frames / 22.05 kHz, five percent is roughly 0.0093
    // project steps per block, below one quarter of a 96-PPQN tick.
    static constexpr double kPhaseErrorGain = 0.125;
    static constexpr double kMaximumTempoTrim = 0.05;
    static constexpr double kMaximumCorrectionSteps = 1.0 / 96.0;

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
        applyBoundedPhaseLock(result, result.sourceChanged);
        return result;
    }

    uint32_t failureCount() const { return failureCount_; }
    const ExternalMidiClockTracker& tracker() const { return tracker_; }

private:
    static double wrapProjectPhaseError(double errorSteps) {
        while (errorSteps > kProjectStepsPerBar * 0.5) {
            errorSteps -= kProjectStepsPerBar;
        }
        while (errorSteps < -kProjectStepsPerBar * 0.5) {
            errorSteps += kProjectStepsPerBar;
        }
        return errorSteps;
    }

    static uint32_t bpmToQ16(double bpm) {
        if (bpm < 5.0) bpm = 5.0;
        if (bpm > 300.0) bpm = 300.0;
        return static_cast<uint32_t>(std::llround(bpm * 65536.0));
    }

    void applyBoundedPhaseLock(ExternalClockBlockResult& result,
                               bool sourceChanged) const {
        ExternalClockEstimate& estimate = result.estimate;
        estimate.phaseErrorSteps = 0.0;
        estimate.phaseCorrectionSteps = 0.0;
        if (sourceChanged || result.command == ExternalTransportCommand::Start ||
            result.command == ExternalTransportCommand::Stop ||
            !estimate.transportRunning || !estimate.validTempo ||
            (estimate.state != ExternalClockLockState::Locked &&
             estimate.state != ExternalClockLockState::Hold)) {
            return;
        }

        const ProjectTransportBlockSnapshot local =
            projectTransportTimeline().snapshot();
        if (!local.valid || !local.playing || local.blockFrames == 0 ||
            local.sampleRate == 0 || local.bpmQ16 == 0) {
            return;
        }

        const double localBpm = local.bpm();
        const double renderedSteps =
            static_cast<double>(local.blockFrames) * localBpm *
            kProjectStepsPerQuarter /
            (60.0 * static_cast<double>(local.sampleRate));
        const double localAtNextBlock = local.absoluteSteps() + renderedSteps;
        const double error = wrapProjectPhaseError(
            estimate.absoluteProjectSteps - localAtNextBlock);
        estimate.phaseErrorSteps = error;

        const double sourceBpm =
            static_cast<double>(estimate.sourceBpmQ16) / 65536.0;
        const double naturalSteps =
            static_cast<double>(local.blockFrames) * sourceBpm *
            kProjectStepsPerQuarter /
            (60.0 * static_cast<double>(local.sampleRate));
        if (naturalSteps <= 0.0) return;

        const double maximumCorrection = std::min(
            kMaximumCorrectionSteps,
            naturalSteps * kMaximumTempoTrim);
        const double correction = std::max(
            -maximumCorrection,
            std::min(maximumCorrection, error * kPhaseErrorGain));
        estimate.phaseCorrectionSteps = correction;

        const double driveBpm = sourceBpm *
            (1.0 + correction / naturalSteps);
        estimate.bpmQ16 = bpmToQ16(driveBpm);
    }

    ExternalMidiClockTracker tracker_;
    TransportClockSource source_{TransportClockSource::GroovePuterInternal};
    uint32_t failureCount_{0};
    bool haveSource_{false};
};

}  // namespace GroovePuterMidi
