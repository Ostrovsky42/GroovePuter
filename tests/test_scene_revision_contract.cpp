#include <cassert>
#include <cstdint>
#include <iostream>

#include "../src/state/scene_revision.h"

int main() {
  using namespace GroovePuterState;

  restoreSceneRevision(SceneRevisionState{});
  assert(!sceneDirty());
  assert(sceneRevisionState().currentRevision == 0);
  assert(sceneRevisionState().persistedRevision == 0);

  markSceneMutated();
  assert(sceneDirty());
  assert(sceneRevisionState().currentRevision == 1);
  assert(sceneRevisionState().persistedRevision == 0);

  const SceneRevisionState beforeFailedTransaction = sceneRevisionSnapshot();
  markSceneMutated();
  assert(sceneRevisionState().currentRevision == 2);
  restoreSceneRevision(beforeFailedTransaction);
  assert(sceneRevisionState().currentRevision == 1);
  assert(sceneDirty());

  markSceneSaveSucceeded();
  assert(!sceneDirty());
  assert(sceneRevisionState().persistedRevision ==
         sceneRevisionState().currentRevision);

  markSceneMutated();
  assert(sceneDirty());
  const uint32_t beforeLoad = sceneRevisionState().currentRevision;
  markSceneLoadSucceeded();
  assert(!sceneDirty());
  assert(sceneRevisionState().currentRevision == beforeLoad + 1);
  assert(sceneRevisionState().persistedRevision ==
         sceneRevisionState().currentRevision);

  SceneRevisionState wrapState{};
  wrapState.currentRevision = UINT32_MAX;
  wrapState.persistedRevision = 0;
  restoreSceneRevision(wrapState);
  markSceneMutated();
  assert(sceneDirty());
  assert(sceneRevisionState().currentRevision !=
         sceneRevisionState().persistedRevision);

  std::cout << "Scene revision contract passed\n";
  return 0;
}
