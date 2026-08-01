#include <cassert>
#include <cstdint>
#include <limits>

#include "src/midi/project_smf_dispatch_policy.h"

using namespace GroovePuterMidi;

int main() {
    constexpr uint32_t blockMicros = 23219;
    constexpr uint32_t now = 1000000;

    assert(!projectTimelineIsStale(
        100, 100, now + blockMicros, now, blockMicros, 2));
    assert(!projectTimelineIsStale(
        100, 100, now, now, blockMicros, 2));
    assert(!projectTimelineIsStale(
        100, 101, now - blockMicros, now, blockMicros, 2));
    assert(!projectTimelineIsStale(
        100, 102, now - blockMicros * 2, now, blockMicros, 2));
    assert(projectTimelineIsStale(
        100, 103, now - blockMicros * 3, now, blockMicros, 2));

    // Signed uint32_t subtraction must also remain correct across micros()
    // rollover for anchors in both the recent past and valid future.
    const uint32_t nearWrap = std::numeric_limits<uint32_t>::max() - 1000u;
    assert(!projectTimelineIsStale(
        10, 10, nearWrap + blockMicros, nearWrap, blockMicros, 2));

    ScheduledSmfMidiEvent original{};
    ProjectTransportBlockSnapshot stopped{};
    assert(projectSmfNoteOnStillCurrent(original, stopped));

    ScheduledSmfMidiEvent project{};
    project.projectTransportEpoch =
        scheduledSmfMidiEventTransportEpochTag(7);
    ProjectTransportBlockSnapshot running{};
    running.valid = true;
    running.playing = true;
    running.transportEpoch = 7;
    assert(projectSmfNoteOnStillCurrent(project, running));

    running.playing = false;
    assert(!projectSmfNoteOnStillCurrent(project, running));
    running.playing = true;
    running.transportEpoch = 8;
    assert(!projectSmfNoteOnStillCurrent(project, running));
    running.valid = false;
    assert(!projectSmfNoteOnStillCurrent(project, running));
    return 0;
}
