#include "src/state/scene_revision.h"

#include <cassert>

int main() {
  using GroovePuterState::SceneRevisionState;

  SceneRevisionState state{};
  assert(!state.dirty());

  // A user mutation makes the Scene dirty. A failed Save performs no success
  // transition, so the dirty state must remain unchanged.
  state.markPersistentMutation();
  const SceneRevisionState failedSaveBefore = state;
  assert(state.dirty());
  assert(state.currentRevision == failedSaveBefore.currentRevision);
  assert(state.persistedRevision == failedSaveBefore.persistedRevision);
  assert(state.dirty());

  // Successful explicit Save catches the persisted baseline up to current.
  state.markSaveSucceeded();
  assert(!state.dirty());
  assert(state.persistedRevision == state.currentRevision);

  // A later mutation followed by failed Load must leave both revision counters
  // untouched; failed operations simply do not invoke the success transition.
  state.markPersistentMutation();
  const SceneRevisionState failedLoadBefore = state;
  assert(state.dirty());
  assert(state.currentRevision == failedLoadBefore.currentRevision);
  assert(state.persistedRevision == failedLoadBefore.persistedRevision);

  // Recovery/autosave is not an explicit Save: writing recovery data performs
  // no revision-success transition and the Scene stays dirty.
  const SceneRevisionState recoveryBefore = state;
  assert(state.dirty());
  assert(state.currentRevision == recoveryBefore.currentRevision);
  assert(state.persistedRevision == recoveryBefore.persistedRevision);
  assert(state.dirty());

  // Successful explicit Load installs a new clean baseline and advances the
  // current revision so observers can see that a new Scene was installed.
  const uint32_t beforeLoadRevision = state.currentRevision;
  state.markLoadSucceeded();
  assert(!state.dirty());
  assert(state.currentRevision != beforeLoadRevision);
  assert(state.persistedRevision == state.currentRevision);

  return 0;
}
