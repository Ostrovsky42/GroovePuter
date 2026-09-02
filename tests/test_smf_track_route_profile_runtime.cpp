#include <cassert>

#include "src/midi/smf_track_route_profile_runtime.h"

using namespace GroovePuterMidi;

int main() {
    SmfTrackRouteProfileIdentity identity{};
    identity.pathHash = 1u;
    identity.contentHash = 2u;
    identity.fileSize = 1024u;
    identity.trackCount = 3u;

    SmfTrackRouteProfileRuntime runtime;
    assert(!runtime.readyFor(7u));
    assert(!runtime.requestLoad(SmfTrackRouteProfileIdentity{}, 7u));
    assert(runtime.requestLoad(identity, 7u));
    assert(!runtime.readyFor(7u));
    assert(!runtime.requestSave(7u));

    SmfTrackRouteProfileIdentity pending{};
    uint32_t generation = 0u;
    assert(runtime.takeLoadRequest(pending, generation));
    assert(generation == 7u);
    assert(pending.matches(identity));
    assert(!runtime.takeLoadRequest(pending, generation));

    runtime.completeLoad(6u, true);
    assert(!runtime.readyFor(7u));
    runtime.completeLoad(7u, true);
    assert(runtime.readyFor(7u));
    assert(runtime.requestSave(7u));
    assert(runtime.takeSaveRequest(pending, generation));
    assert(generation == 7u);
    assert(pending.matches(identity));
    assert(!runtime.takeSaveRequest(pending, generation));

    SmfTrackRouteProfileIdentity next = identity;
    next.contentHash = 3u;
    assert(runtime.requestLoad(next, 8u));
    assert(!runtime.readyFor(7u));
    assert(!runtime.readyFor(8u));
    assert(!runtime.requestSave(7u));
    assert(runtime.takeLoadRequest(pending, generation));
    assert(generation == 8u);
    assert(pending.matches(next));
    runtime.completeLoad(8u, false);
    assert(!runtime.readyFor(8u));

    return 0;
}
