#include <cassert>
#include <cmath>

#include "src/midi/external_midi_clock_tracker.h"

using namespace GroovePuterMidi;

int main() {
    ExternalMidiClockTracker tracker;
    const uint32_t startMicros = 1000000;

    // Some masters send FA before any pre-roll F8. The first following Clock is
    // the downbeat itself: it establishes the timing anchor and position 0.
    // Measured on SEQTRAK (2026-10-06, two Starts): its step-1 note left 5-6 ms
    // after the first F8 following FA, one whole pulse before the second F8.
    // Counting that first F8 as +1/6 step made GroovePuter play one pulse
    // ahead of the master. GroovePuter's own output also puts FA and the first
    // F8 on step 0.
    tracker.onStart(startMicros);
    assert(tracker.transportRunning());
    assert(!tracker.onClock(startMicros + 20833u, 1u));
    const auto first = tracker.estimate(startMicros + 20833u);
    assert(std::fabs(first.absoluteProjectSteps) < 1.0e-9);

    // The second pulse after Start is the first real 1/24-quarter advance.
    tracker.onClock(startMicros + 2u * 20833u, 2u);
    const auto second = tracker.estimate(startMicros + 2u * 20833u);
    assert(std::fabs(second.absoluteProjectSteps -
                     ExternalMidiClockTracker::kProjectStepsPerClockPulse) <
           1.0e-9);

    // A pre-locked master behaves the same: the stopped Clocks are timing
    // anchors only, and the first post-Start pulse is position 0. Between
    // Start and that pulse the position does not run ahead on prediction.
    ExternalMidiClockTracker prelocked;
    uint32_t now = 2000000;
    uint32_t ordinal = 1;
    prelocked.onClock(now, ordinal);
    for (int i = 0; i < 6; ++i) {
        now += 20833u;
        prelocked.onClock(now, ++ordinal);
    }
    assert(prelocked.state() == ExternalClockLockState::Locked);
    prelocked.onStart(now);
    assert(std::fabs(prelocked.estimate(now + 15000u).absoluteProjectSteps) <
           1.0e-9);
    now += 20833u;
    prelocked.onClock(now, ++ordinal);
    assert(std::fabs(prelocked.estimate(now).absoluteProjectSteps) < 0.001);
    now += 20833u;
    prelocked.onClock(now, ++ordinal);
    const auto afterStart = prelocked.estimate(now);
    assert(std::fabs(afterStart.absoluteProjectSteps -
                     ExternalMidiClockTracker::kProjectStepsPerClockPulse) <
           0.001);

    return 0;
}
