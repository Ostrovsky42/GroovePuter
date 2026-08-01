#include <cassert>
#include <cmath>
#include <cstdint>

#include "src/midi/external_midi_clock_tracker.h"

using namespace GroovePuterMidi;

namespace {
double bpm(const ExternalMidiClockTracker& tracker) {
    return static_cast<double>(tracker.bpmQ16()) / 65536.0;
}

bool closeEnough(double a, double b, double epsilon) {
    return std::fabs(a - b) <= epsilon;
}

void feedFixed(ExternalMidiClockTracker& tracker,
               uint32_t& now,
               uint32_t& ordinal,
               uint32_t period,
               uint32_t pulses) {
    for (uint32_t i = 0; i < pulses; ++i) {
        now += period;
        ++ordinal;
        tracker.onClock(now, ordinal);
    }
}
}

int main() {
    ExternalMidiClockTracker tracker;
    uint32_t now = 1000000;
    uint32_t ordinal = 0;

    assert(tracker.state() == ExternalClockLockState::Waiting);
    tracker.onClock(now, ++ordinal);
    assert(tracker.state() == ExternalClockLockState::Locking);
    feedFixed(tracker, now, ordinal, 20833, 6);
    assert(tracker.state() == ExternalClockLockState::Locked);
    assert(closeEnough(bpm(tracker), 120.0, 0.05));

    // Clock before Start may lock tempo but must not run or move project phase.
    const auto stoppedEstimate = tracker.estimate(now);
    assert(!stoppedEstimate.transportRunning);
    assert(closeEnough(stoppedEstimate.absoluteProjectSteps, 0.0, 1.0e-9));
    assert(stoppedEstimate.sourceBpmQ16 == stoppedEstimate.bpmQ16);

    tracker.onStart(now);
    assert(tracker.transportRunning());
    assert(tracker.transportEpoch() == 1);
    feedFixed(tracker, now, ordinal, 20833, 6);
    const auto running = tracker.estimate(now);
    assert(closeEnough(running.absoluteProjectSteps, 1.0, 0.001));

    tracker.onStop(now);
    const double stoppedAt = tracker.estimate(now).absoluteProjectSteps;
    assert(!tracker.transportRunning());
    feedFixed(tracker, now, ordinal, 20833, 3);
    assert(closeEnough(
        tracker.estimate(now).absoluteProjectSteps, stoppedAt, 0.001));

    tracker.onContinue(now);
    assert(tracker.transportRunning());
    feedFixed(tracker, now, ordinal, 20833, 3);
    assert(closeEnough(
        tracker.estimate(now).absoluteProjectSteps,
        stoppedAt + 0.5,
        0.001));

    // Missing one queued F8 is represented by pulseOrdinal and does not become
    // a false half-tempo interval.
    now += 41666;
    ordinal += 2;
    assert(tracker.onClock(now, ordinal));
    assert(tracker.pulseGapCount() == 1);
    assert(closeEnough(bpm(tracker), 120.0, 0.1));

    // Typical USB scheduling jitter remains locked and filtered.
    const int32_t jitter[] = {-700, 450, -300, 900, -850, 200, 0, 500};
    for (int32_t delta : jitter) {
        now += static_cast<uint32_t>(20833 + delta);
        tracker.onClock(now, ++ordinal);
    }
    assert(tracker.state() == ExternalClockLockState::Locked);
    assert(closeEnough(bpm(tracker), 120.0, 1.0));

    // A tempo change converges without resetting the transport position.
    const double beforeTempoChange = tracker.estimate(now).absoluteProjectSteps;
    feedFixed(tracker, now, ordinal, 22727, 32);  // 110 BPM
    assert(tracker.transportRunning());
    assert(tracker.estimate(now).absoluteProjectSteps > beforeTempoChange);
    assert(closeEnough(bpm(tracker), 110.0, 1.0));

    const uint32_t period = tracker.filteredPulsePeriodUs();
    tracker.update(now + std::max<uint32_t>(period * 2u, 100000u) + 1u);
    assert(tracker.state() == ExternalClockLockState::Hold);
    assert(tracker.holdTransitionCount() == 1);

    tracker.update(now + std::max<uint32_t>(period * 4u, 250000u) + 1u);
    assert(tracker.state() == ExternalClockLockState::Lost);
    assert(!tracker.transportRunning());
    assert(tracker.lostTransitionCount() == 1);

    tracker.onStart(now);
    assert(tracker.transportEpoch() == 2);
    assert(closeEnough(
        tracker.estimate(now).absoluteProjectSteps, 0.0, 1.0e-9));

    // A rejected timestamp interval cannot advance musical phase even though
    // its ordinal is consumed for subsequent gap reconstruction.
    ExternalMidiClockTracker outlier;
    uint32_t outlierNow = 2000000;
    uint32_t outlierOrdinal = 0;
    outlier.onClock(outlierNow, ++outlierOrdinal);
    feedFixed(outlier, outlierNow, outlierOrdinal, 20833, 6);
    outlier.onStart(outlierNow);
    feedFixed(outlier, outlierNow, outlierOrdinal, 20833, 2);
    const double beforeOutlier =
        outlier.estimate(outlierNow).absoluteProjectSteps;
    ++outlierOrdinal;
    assert(!outlier.onClock(outlierNow + 1u, outlierOrdinal));
    assert(closeEnough(
        outlier.estimate(outlierNow + 1u).absoluteProjectSteps,
        beforeOutlier,
        1.0e-9));
    assert(outlier.intervalOutlierCount() == 1);

    // Range endpoints remain valid.
    ExternalMidiClockTracker slow;
    uint32_t slowNow = 0;
    uint32_t slowOrdinal = 0;
    slow.onClock(slowNow, ++slowOrdinal);
    feedFixed(slow, slowNow, slowOrdinal, 500000, 6);
    assert(slow.state() == ExternalClockLockState::Locked);
    assert(closeEnough(bpm(slow), 5.0, 0.01));

    ExternalMidiClockTracker fast;
    uint32_t fastNow = 0;
    uint32_t fastOrdinal = 0;
    fast.onClock(fastNow, ++fastOrdinal);
    feedFixed(fast, fastNow, fastOrdinal, 8333, 6);
    assert(fast.state() == ExternalClockLockState::Locked);
    assert(closeEnough(bpm(fast), 300.0, 0.1));

    fast.onFailure(fastNow);
    assert(fast.state() == ExternalClockLockState::Lost);
    assert(!fast.transportRunning());
    assert(fast.lostTransitionCount() == 1);

    return 0;
}
