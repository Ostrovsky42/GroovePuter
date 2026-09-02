#include <cassert>
#include <cstdint>

#include "src/midi/scheduled_smf_midi_event_queue.h"
#include "src/midi/smf_session_generation.h"
#include "src/midi/smf_track_level.h"
#include "src/midi/smf_track_mute.h"

int main() {
    using namespace GroovePuterMidi;

    const uint32_t generation1 = smfBeginSessionOpen();
    assert(generation1 != 0u);
    assert(smfCompleteSessionOpen(generation1));
    smfTrackMuteState().reset(64);

    SmfTrackLevelState& levels = smfTrackLevelState();
    assert(levels.levelFor(7) == 100u);

    uint8_t result = 0u;
    assert(levels.adjustLevel(7, -50, generation1, result));
    assert(result == 50u);
    assert(levels.levelFor(7) == 50u);
    assert(applySmfTrackLevelVelocity(101u, 50u) == 51u);
    assert(applySmfTrackLevelVelocity(1u, 1u) == 1u);

    ScheduledSmfMidiEventQueue queue;
    assert(queue.tryPushNoteOn(8, 60, 101, 10, 0, 0, 7));
    ScheduledSmfMidiEvent event{};
    assert(queue.tryPop(event));
    assert(event.type == ScheduledSmfMidiEventType::NoteOn);
    assert(event.trackIndex == 7u);
    assert(event.velocity == 51u);

    assert(levels.setLevel(7, 0u, generation1));
    assert(levels.levelFor(7) == 0u);
    assert(queue.tryPushNoteOn(8, 61, 100, 11, 0, 0, 7));
    assert(!queue.tryPop(event));

    assert(levels.setLevel(7, 100u, generation1));
    assert(queue.tryPushNoteOn(8, 62, 100, 12, 0, 0, 7));
    assert(queue.tryPop(event));
    assert(event.velocity == 100u);

    const uint32_t generation2 = smfBeginSessionOpen();
    assert(generation2 != 0u && generation2 != generation1);
    assert(smfCompleteSessionOpen(generation2));
    assert(levels.levelFor(7) == 100u);

    return 0;
}
