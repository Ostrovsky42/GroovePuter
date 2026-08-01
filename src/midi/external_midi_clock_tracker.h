#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

enum class ExternalClockLockState : uint8_t {
    Waiting = 0,
    Locking,
    Locked,
    Hold,
    Lost,
};

struct ExternalClockEstimate {
    ExternalClockLockState state{ExternalClockLockState::Waiting};
    bool transportRunning{false};
    bool validTempo{false};
    // bpmQ16 is the bounded drive tempo applied to the local sequencer. The
    // source value remains separate so UI/diagnostics report SEQTRAK tempo
    // rather than the small PLL correction.
    uint32_t bpmQ16{0};
    uint32_t sourceBpmQ16{0};
    uint32_t filteredPulsePeriodUs{0};
    uint32_t transportEpoch{0};
    uint64_t pulseCount{0};
    double absoluteProjectSteps{0.0};
    double phaseErrorSteps{0.0};
    double phaseCorrectionSteps{0.0};
};

// Tracks USB MIDI Timing Clock without allocating or touching the audio engine.
// MidiDispatchTask timestamps F8 pulses; AudioTask owns this tracker and applies
// its estimate only at render-block boundaries.
class ExternalMidiClockTracker {
public:
    static constexpr uint32_t kMinimumPulsePeriodUs = 8000;
    static constexpr uint32_t kMaximumPulsePeriodUs = 550000;
    static constexpr uint32_t kIntervalsToLock = 6;
    static constexpr std::size_t kMedianWindow = 5;
    static constexpr double kProjectStepsPerClockPulse = 1.0 / 6.0;

    void reset() {
        *this = ExternalMidiClockTracker{};
    }

    bool onClock(uint32_t timestampMicros, uint32_t pulseOrdinal) {
        ++pulseCount_;
        if (!havePhasePulse_) {
            havePhasePulse_ = true;
            haveTimingPulse_ = true;
            lastPhasePulseOrdinal_ = pulseOrdinal;
            lastTimingPulseOrdinal_ = pulseOrdinal;
            lastTimingPulseMicros_ = timestampMicros;
            lastObservedPulseMicros_ = timestampMicros;
            if (state_ == ExternalClockLockState::Waiting) {
                state_ = ExternalClockLockState::Locking;
            }
            return false;
        }

        const uint32_t phaseGap = pulseOrdinal - lastPhasePulseOrdinal_;
        if (phaseGap == 0) {
            ++intervalOutliers_;
            return false;
        }
        lastPhasePulseOrdinal_ = pulseOrdinal;
        lastObservedPulseMicros_ = timestampMicros;
        if (phaseGap > 1) pulseGaps_ += phaseGap - 1u;
        if (transportRunning_) {
            absoluteProjectSteps_ +=
                static_cast<double>(phaseGap) * kProjectStepsPerClockPulse;
        }

        if (!haveTimingPulse_) {
            haveTimingPulse_ = true;
            lastTimingPulseOrdinal_ = pulseOrdinal;
            lastTimingPulseMicros_ = timestampMicros;
            return false;
        }

        const uint32_t timingGap = pulseOrdinal - lastTimingPulseOrdinal_;
        const uint32_t elapsedUs = timestampMicros - lastTimingPulseMicros_;
        if (timingGap == 0 || elapsedUs == 0) {
            ++intervalOutliers_;
            return false;
        }

        const uint32_t perPulseUs = static_cast<uint32_t>(
            (static_cast<uint64_t>(elapsedUs) + timingGap / 2u) /
            timingGap);
        if (perPulseUs < kMinimumPulsePeriodUs ||
            perPulseUs > kMaximumPulsePeriodUs) {
            // The F8 pulse remains musically real and has already advanced the
            // phase ordinal, but its timestamp is not allowed to poison the
            // accepted tempo anchor or median window.
            ++intervalOutliers_;
            consecutiveValidIntervals_ = 0;
            if (state_ != ExternalClockLockState::Lost) {
                state_ = ExternalClockLockState::Locking;
            }
            return false;
        }

        lastTimingPulseOrdinal_ = pulseOrdinal;
        lastTimingPulseMicros_ = timestampMicros;
        intervals_[intervalWriteIndex_] = perPulseUs;
        intervalWriteIndex_ = (intervalWriteIndex_ + 1u) % kMedianWindow;
        if (intervalCount_ < kMedianWindow) ++intervalCount_;

        const uint32_t median = medianInterval();
        if (filteredPulsePeriodUs_ == 0) {
            filteredPulsePeriodUs_ = median;
        } else {
            const int64_t delta = static_cast<int64_t>(median) -
                                  filteredPulsePeriodUs_;
            filteredPulsePeriodUs_ = static_cast<uint32_t>(
                static_cast<int64_t>(filteredPulsePeriodUs_) + delta / 8);
        }

        if (consecutiveValidIntervals_ < UINT32_MAX) {
            ++consecutiveValidIntervals_;
        }
        state_ = consecutiveValidIntervals_ >= kIntervalsToLock
            ? ExternalClockLockState::Locked
            : ExternalClockLockState::Locking;
        return true;
    }

    void onStart(uint32_t timestampMicros) {
        (void)timestampMicros;
        ++transportEpoch_;
        transportRunning_ = true;
        absoluteProjectSteps_ = 0.0;
        ++startCount_;
    }

    void onContinue(uint32_t timestampMicros) {
        (void)timestampMicros;
        transportRunning_ = true;
        ++continueCount_;
    }

    void onStop(uint32_t timestampMicros) {
        (void)timestampMicros;
        transportRunning_ = false;
        ++stopCount_;
    }

    void onFailure(uint32_t timestampMicros) {
        (void)timestampMicros;
        if (state_ != ExternalClockLockState::Lost) ++lostTransitions_;
        state_ = ExternalClockLockState::Lost;
        transportRunning_ = false;
    }

    void update(uint32_t nowMicros) {
        if (!havePhasePulse_ || filteredPulsePeriodUs_ == 0) return;
        const uint32_t elapsedUs = nowMicros - lastObservedPulseMicros_;
        const uint32_t holdTimeoutUs = std::max<uint32_t>(
            filteredPulsePeriodUs_ * 2u, 100000u);
        const uint32_t lostTimeoutUs = std::min<uint32_t>(
            2500000u,
            std::max<uint32_t>(filteredPulsePeriodUs_ * 4u, 250000u));

        if (elapsedUs > lostTimeoutUs) {
            if (state_ != ExternalClockLockState::Lost) ++lostTransitions_;
            state_ = ExternalClockLockState::Lost;
            transportRunning_ = false;
            return;
        }
        if (elapsedUs > holdTimeoutUs &&
            state_ == ExternalClockLockState::Locked) {
            state_ = ExternalClockLockState::Hold;
            ++holdTransitions_;
        }
    }

    ExternalClockEstimate estimate(uint32_t nowMicros) const {
        ExternalClockEstimate out{};
        out.state = state_;
        out.transportRunning = transportRunning_;
        out.validTempo = filteredPulsePeriodUs_ != 0;
        out.sourceBpmQ16 = bpmQ16();
        out.bpmQ16 = out.sourceBpmQ16;
        out.filteredPulsePeriodUs = filteredPulsePeriodUs_;
        out.transportEpoch = transportEpoch_;
        out.pulseCount = pulseCount_;
        out.absoluteProjectSteps = predictedAbsoluteProjectSteps(nowMicros);
        return out;
    }

    uint32_t filteredPulsePeriodUs() const {
        return filteredPulsePeriodUs_;
    }

    uint32_t bpmQ16() const {
        if (filteredPulsePeriodUs_ == 0) return 0;
        return static_cast<uint32_t>(
            (60000000ull << 16) /
            (static_cast<uint64_t>(filteredPulsePeriodUs_) * 24ull));
    }

    double predictedAbsoluteProjectSteps(uint32_t nowMicros) const {
        if (!transportRunning_ || filteredPulsePeriodUs_ == 0 ||
            state_ == ExternalClockLockState::Lost) {
            return absoluteProjectSteps_;
        }
        const uint32_t elapsedUs = nowMicros - lastObservedPulseMicros_;
        const double predictedPulses =
            static_cast<double>(elapsedUs) / filteredPulsePeriodUs_;
        return absoluteProjectSteps_ +
               predictedPulses * kProjectStepsPerClockPulse;
    }

    ExternalClockLockState state() const { return state_; }
    bool transportRunning() const { return transportRunning_; }
    uint32_t transportEpoch() const { return transportEpoch_; }
    uint32_t pulseGapCount() const { return pulseGaps_; }
    uint32_t intervalOutlierCount() const { return intervalOutliers_; }
    uint32_t holdTransitionCount() const { return holdTransitions_; }
    uint32_t lostTransitionCount() const { return lostTransitions_; }
    uint32_t startCount() const { return startCount_; }
    uint32_t continueCount() const { return continueCount_; }
    uint32_t stopCount() const { return stopCount_; }

private:
    uint32_t medianInterval() const {
        std::array<uint32_t, kMedianWindow> sorted{};
        for (std::size_t i = 0; i < intervalCount_; ++i) {
            sorted[i] = intervals_[i];
        }
        std::sort(sorted.begin(), sorted.begin() + intervalCount_);
        return sorted[intervalCount_ / 2u];
    }

    ExternalClockLockState state_{ExternalClockLockState::Waiting};
    std::array<uint32_t, kMedianWindow> intervals_{};
    std::size_t intervalWriteIndex_{0};
    std::size_t intervalCount_{0};
    uint32_t filteredPulsePeriodUs_{0};
    uint32_t consecutiveValidIntervals_{0};
    uint32_t lastTimingPulseMicros_{0};
    uint32_t lastObservedPulseMicros_{0};
    uint32_t lastTimingPulseOrdinal_{0};
    uint32_t lastPhasePulseOrdinal_{0};
    uint32_t transportEpoch_{0};
    uint64_t pulseCount_{0};
    double absoluteProjectSteps_{0.0};
    bool haveTimingPulse_{false};
    bool havePhasePulse_{false};
    bool transportRunning_{false};
    uint32_t pulseGaps_{0};
    uint32_t intervalOutliers_{0};
    uint32_t holdTransitions_{0};
    uint32_t lostTransitions_{0};
    uint32_t startCount_{0};
    uint32_t continueCount_{0};
    uint32_t stopCount_{0};
};

inline constexpr const char* externalClockLockStateName(
        ExternalClockLockState state) {
    switch (state) {
        case ExternalClockLockState::Waiting: return "WAIT";
        case ExternalClockLockState::Locking: return "LOCKING";
        case ExternalClockLockState::Locked: return "LOCKED";
        case ExternalClockLockState::Hold: return "HOLD";
        case ExternalClockLockState::Lost: return "LOST";
    }
    return "WAIT";
}

}  // namespace GroovePuterMidi
