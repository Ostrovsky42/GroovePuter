#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "external_midi_clock_tracker.h"
#include "external_midi_transport_event_queue.h"
#include "project_transport_timeline.h"
#include "transport_clock_runtime.h"
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
    bool followChanged{false};
    bool queueFailure{false};
};

class ExternalMidiClockFollower {
public:
    static constexpr double kMaximumTempoTrim = 0.02;
    static constexpr double kMaximumCorrectionSteps = 1.0 / 96.0;
    static constexpr double kPhaseTrimEnterSteps = 1.0 / 32.0;
    static constexpr double kPhaseTrimExitSteps = 1.0 / 96.0;
    static constexpr double kSourceBpmQuantum = 0.1;
    static constexpr double kSourceBpmHysteresis = 0.15;
    static constexpr uint32_t kClockCoalesceWindowUs = 1000;

    ExternalClockBlockResult processBlock(
            ExternalMidiTransportEventQueue& queue,
            TransportClockSource source,
            uint32_t nowMicros,
            bool followEnabled =
                transportClockRuntime().externalFollowEnabled()) {
        ExternalClockBlockResult result{};
        if (!haveSource_ || source != source_) {
            source_ = source;
            haveSource_ = true;
            tracker_.reset();
            driveBaseBpmQ16_ = 0;
            phaseTrimDirection_ = 0;
            queue.clearFailure();
            result.sourceChanged = true;
            result.command = ExternalTransportCommand::Stop;
            if (source != TransportClockSource::SeqtrakExternal) {
                queue.discardPending();
            }
        }

        if (!haveFollowState_ || followEnabled != followEnabled_) {
            const bool wasEnabled = haveFollowState_ && followEnabled_;
            followEnabled_ = followEnabled;
            haveFollowState_ = true;
            result.followChanged = true;
            phaseTrimDirection_ = 0;
            if (wasEnabled && !followEnabled_) {
                tracker_.onStop(nowMicros);
                result.command = ExternalTransportCommand::Stop;
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
            driveBaseBpmQ16_ = 0;
            phaseTrimDirection_ = 0;
            ++failureCount_;
            result.command = ExternalTransportCommand::Stop;
            result.queueFailure = true;
            result.estimate = tracker_.estimate(nowMicros);
            return result;
        }

        ExternalMidiTransportEvent pendingClock{};
        uint32_t pendingClockCount = 0;
        bool havePendingClock = false;
        auto flushPendingClock = [&]() {
            if (!havePendingClock) return;
            tracker_.onClock(pendingClock.timestampMicros,
                             pendingClock.pulseOrdinal,
                             pendingClockCount);
            havePendingClock = false;
            pendingClockCount = 0;
        };

        ExternalMidiTransportEvent event{};
        while (queue.tryPop(event)) {
            if (event.type == ExternalMidiTransportEventType::Clock) {
                if (havePendingClock &&
                    static_cast<uint32_t>(event.timestampMicros -
                                          pendingClock.timestampMicros) >
                        kClockCoalesceWindowUs) {
                    flushPendingClock();
                }
                pendingClock = event;
                ++pendingClockCount;
                havePendingClock = true;
                continue;
            }

            flushPendingClock();
            switch (event.type) {
                case ExternalMidiTransportEventType::Clock:
                    break;
                case ExternalMidiTransportEventType::Start:
                    if (followEnabled_) {
                        tracker_.onStart(event.timestampMicros);
                        result.command = ExternalTransportCommand::Start;
                    }
                    break;
                case ExternalMidiTransportEventType::Continue:
                    if (followEnabled_) {
                        tracker_.onContinue(event.timestampMicros);
                        result.command = ExternalTransportCommand::Continue;
                    }
                    break;
                case ExternalMidiTransportEventType::Stop:
                    tracker_.onStop(event.timestampMicros);
                    if (followEnabled_) {
                        result.command = ExternalTransportCommand::Stop;
                    }
                    break;
            }
        }
        flushPendingClock();

        const bool wasRunning = tracker_.transportRunning();
        tracker_.update(nowMicros);
        if (wasRunning && !tracker_.transportRunning() &&
            tracker_.state() == ExternalClockLockState::Lost) {
            result.command = ExternalTransportCommand::Stop;
        }
        result.estimate = tracker_.estimate(nowMicros);
        applyBoundedPhaseLock(result, result.sourceChanged || result.followChanged);
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

    static double q16ToBpm(uint32_t bpmQ16) {
        return static_cast<double>(bpmQ16) / 65536.0;
    }

    bool updateDriveBase(uint32_t sourceBpmQ16, bool force) {
        if (sourceBpmQ16 == 0) return false;
        const double sourceBpm = q16ToBpm(sourceBpmQ16);
        const double quantized =
            std::round(sourceBpm / kSourceBpmQuantum) * kSourceBpmQuantum;
        if (!force && driveBaseBpmQ16_ != 0 &&
            std::fabs(quantized - q16ToBpm(driveBaseBpmQ16_)) <
                kSourceBpmHysteresis) {
            return false;
        }
        const uint32_t next = bpmToQ16(quantized);
        if (next == driveBaseBpmQ16_) return false;
        driveBaseBpmQ16_ = next;
        phaseTrimDirection_ = 0;
        return true;
    }

    void updateTrimDirection(double errorSteps) {
        if (phaseTrimDirection_ == 0) {
            if (errorSteps > kPhaseTrimEnterSteps) phaseTrimDirection_ = 1;
            else if (errorSteps < -kPhaseTrimEnterSteps) phaseTrimDirection_ = -1;
            return;
        }
        if (phaseTrimDirection_ > 0) {
            if (errorSteps < -kPhaseTrimEnterSteps) phaseTrimDirection_ = -1;
            else if (errorSteps < kPhaseTrimExitSteps) phaseTrimDirection_ = 0;
            return;
        }
        if (errorSteps > kPhaseTrimEnterSteps) phaseTrimDirection_ = 1;
        else if (errorSteps > -kPhaseTrimExitSteps) phaseTrimDirection_ = 0;
    }

    void applyBoundedPhaseLock(ExternalClockBlockResult& result,
                               bool controlChanged) {
        ExternalClockEstimate& estimate = result.estimate;
        estimate.phaseErrorSteps = 0.0;
        estimate.phaseCorrectionSteps = 0.0;

        const bool driveBaseChanged = updateDriveBase(
            estimate.sourceBpmQ16,
            controlChanged || result.command == ExternalTransportCommand::Start);
        if (driveBaseBpmQ16_ != 0) estimate.bpmQ16 = driveBaseBpmQ16_;

        if (controlChanged || driveBaseChanged || !followEnabled_ ||
            result.command == ExternalTransportCommand::Start ||
            result.command == ExternalTransportCommand::Stop ||
            !estimate.transportRunning || !estimate.validTempo ||
            (estimate.state != ExternalClockLockState::Locked &&
             estimate.state != ExternalClockLockState::Hold)) {
            phaseTrimDirection_ = 0;
            return;
        }

        const ProjectTransportBlockSnapshot local =
            projectTransportTimeline().snapshot();
        if (!local.valid || !local.playing || local.blockFrames == 0 ||
            local.sampleRate == 0 || local.bpmQ16 == 0) return;

        const double renderedSteps =
            static_cast<double>(local.blockFrames) * local.bpm() *
            kProjectStepsPerQuarter /
            (60.0 * static_cast<double>(local.sampleRate));
        const double error = wrapProjectPhaseError(
            estimate.absoluteProjectSteps -
            (local.absoluteSteps() + renderedSteps));
        estimate.phaseErrorSteps = error;
        updateTrimDirection(error);

        const double baseBpm = q16ToBpm(driveBaseBpmQ16_);
        const double naturalSteps =
            static_cast<double>(local.blockFrames) * baseBpm *
            kProjectStepsPerQuarter /
            (60.0 * static_cast<double>(local.sampleRate));
        if (naturalSteps <= 0.0) return;

        double correction = naturalSteps * kMaximumTempoTrim *
                            static_cast<double>(phaseTrimDirection_);
        correction = std::max(-kMaximumCorrectionSteps,
                              std::min(kMaximumCorrectionSteps, correction));
        estimate.phaseCorrectionSteps = correction;
        estimate.bpmQ16 = bpmToQ16(
            baseBpm * (1.0 + kMaximumTempoTrim *
                                static_cast<double>(phaseTrimDirection_)));
    }

    ExternalMidiClockTracker tracker_;
    TransportClockSource source_{TransportClockSource::GroovePuterInternal};
    uint32_t driveBaseBpmQ16_{0};
    uint32_t failureCount_{0};
    int8_t phaseTrimDirection_{0};
    bool haveSource_{false};
    bool followEnabled_{true};
    bool haveFollowState_{false};
};

}  // namespace GroovePuterMidi
