#include "../src/state/undo_owner.h"
#include <cassert>
#include <cstdio>

struct TogglePayload { int value; };

int main() {
  using namespace GroovePuterUndo;
  auto& owner = undoOwner();
  owner.clear();
  GroovePuterState::restoreSceneRevision({10, 10});

  int live = 1;
  TogglePayload before{live};
  assert(owner.commitPrepared(UndoKind::Pattern, before, [&] { live = 2; }));
  assert(live == 2);
  assert(!owner.nextIsRedo());
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 11);

  auto exchange = [&](TogglePayload& retained) {
    const int value = live;
    live = retained.value;
    retained.value = value;
  };
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 1);
  assert(owner.hasUndo());
  assert(owner.nextIsRedo());
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 10);

  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 2);
  assert(owner.hasUndo());
  assert(!owner.nextIsRedo());
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 11);

  TogglePayload second{live};
  assert(owner.commitPrepared(UndoKind::Pattern, second, [&] { live = 3; }));
  assert(live == 3);
  assert(!owner.nextIsRedo());
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 2);

  // Saving the undone side makes the inverse transition dirty, not falsely clean.
  GroovePuterState::markSceneSaveSucceeded();
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 3);
  assert(GroovePuterState::sceneDirty());

  static_assert(sizeof(UndoOwner) <= 1552,
      "one-slot toggle must not grow authoritative owner DRAM");
  std::printf("R9 one-slot toggle owner=%zu bytes PASS\n", sizeof(UndoOwner));
  return 0;
}
