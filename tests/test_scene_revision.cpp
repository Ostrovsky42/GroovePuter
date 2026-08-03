#include <cassert>
#include <cstdint>

#include "src/state/scene_revision.h"

using namespace GroovePuterState;

int main() {
    SceneRevisionState state{};
    assert(sizeof(state) == 8);
    assert(!state.dirty());

    state.markPersistentMutation();
    assert(state.dirty());
    assert(state.currentRevision == 1);
    assert(state.persistedRevision == 0);

    state.markSaveSucceeded();
    assert(!state.dirty());
    assert(state.persistedRevision == state.currentRevision);

    state.markPersistentMutation();
    const uint32_t beforeFailedSave = state.currentRevision;
    assert(state.dirty());
    // Failure semantics: no success callback, therefore dirty remains set.
    assert(state.currentRevision == beforeFailedSave);
    assert(state.dirty());

    state.markLoadSucceeded();
    assert(!state.dirty());
    assert(state.currentRevision == state.persistedRevision);

    state.currentRevision = UINT32_MAX;
    state.persistedRevision = 0;
    state.markPersistentMutation();
    assert(state.currentRevision == 1);
    assert(state.dirty());

    return 0;
}
