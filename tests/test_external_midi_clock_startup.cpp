#include <cassert>
#include <cmath>

#include "src/midi/external_midi_clock_tracker.h"

using namespace GroovePuterMidi;

int main() {
    ExternalMidiClockTracker tracker;
    const uint32_t startMicros = 1000000;

    // Some masters send FA before any pre-roll F8. The first following Clock is
    // the first real 1/24-quarter pulse and must advance the running phase even
    // though it also establishes the initial timing anchor.
    tracker.onStart(startMicros);
    assert(tracker.transportRunning());
    assert(!tracker.onClock(startMicros + 20833u, 1u));
    const auto first = tracker.estimate(startMicros + 20833u);
    assert(std::fabs(first.absoluteProjectSteps -
                     ExternalMidiClockTracker::kProjectStepsPerClockPulse) <
           1.0e-9);

    // A pre-locked master still advances exactly once on the first post-Start
    // pulse; the initial stopped Clock remains a timing anchor only.
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
    now += 20833u;
    prelocked.onClock(now, ++ordinal);
    const auto afterStart = prelocked.estimate(now);
    assert(std::fabs(afterStart.absoluteProjectSteps -
                     ExternalMidiClockTracker::kProjectStepsPerClockPulse) <
           0.001);

    return 0;
}
