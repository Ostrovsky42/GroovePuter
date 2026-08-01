#include <cassert>

#include "src/midi/smf_track_mute.h"

using namespace GroovePuterMidi;

int main() {
    SmfTrackMuteState state;
    state.reset(4);

    SmfTrackMuteSnapshot snapshot = state.snapshot();
    assert(snapshot.trackCount == 4);
    assert(snapshot.selectedTrack == 0);
    assert(snapshot.mutedMask == 0);

    state.selectRelative(1);
    assert(state.snapshot().selectedTrack == 1);
    assert(state.toggleSelected());
    assert(state.isMuted(1));
    assert(!state.isMuted(0));

    // Muted NoteOn is suppressed, but NoteOff remains cleanup-critical.
    assert(!shouldEmitSmfTrackEvent(true, 1, state));
    assert(shouldEmitSmfTrackEvent(false, 1, state));
    assert(shouldEmitSmfTrackEvent(true, 0, state));

    state.selectRelative(3);  // wrap 1 + 3 -> 0
    assert(state.snapshot().selectedTrack == 0);
    state.selectRelative(-1); // wrap 0 - 1 -> 3
    assert(state.snapshot().selectedTrack == 3);

    assert(state.toggleSelected());
    assert(state.isMuted(3));
    assert(state.snapshot().mutedMask == ((uint64_t{1} << 1) |
                                           (uint64_t{1} << 3)));

    state.clear();
    assert(state.snapshot().mutedMask == 0);

    state.reset(100);  // hard bounded to the SMF maximum
    assert(state.snapshot().trackCount == 64);
    state.selectRelative(-1);
    assert(state.snapshot().selectedTrack == 63);
    assert(state.toggleSelected());
    assert(state.isMuted(63));

    state.reset(0);
    assert(!state.toggleSelected());
    assert(state.snapshot().trackCount == 0);

    return 0;
}
