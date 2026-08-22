#include "../src/state/undo_owner.h"

#include <cassert>
#include <cstdio>

struct TogglePayload { int value; };

int main() {
  using namespace GroovePuterUndo;
  auto& owner = undoOwner();

  // Basic one-slot toggle: Undo and Redo exchange the same retained pair.
  owner.clear();
  GroovePuterState::restoreSceneRevision({10, 10});
  int live = 1;
  auto exchange = [&](TogglePayload& retained) {
    const int value = live;
    live = retained.value;
    retained.value = value;
  };

  TogglePayload before{live};
  assert(owner.commitPrepared(UndoKind::Pattern, before, [&] { live = 2; }));
  assert(live == 2);
  assert(!owner.nextIsRedo());
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 11);

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

  // Fork rule: any accepted mutation after Undo destroys the old Redo branch.
  // Undo(A) -> edit B -> Ctrl+Z must Undo(B), never Redo(A).
  owner.clear();
  GroovePuterState::restoreSceneRevision({20, 20});
  live = 10;
  TogglePayload aBefore{live};
  assert(owner.commitPrepared(UndoKind::Pattern, aBefore, [&] { live = 11; }));
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 10);
  assert(owner.nextIsRedo());

  TogglePayload bBefore{live};
  assert(owner.commitPrepared(UndoKind::Pattern, bBefore, [&] { live = 42; }));
  assert(live == 42);
  assert(!owner.nextIsRedo());  // publish() reset direction to canonical Undo.

  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 10);           // Undo B.
  assert(owner.nextIsRedo());
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 42);           // Redo B, not stale Redo A (=11).
  assert(!owner.nextIsRedo());

  // Saving the undone side makes the inverse transition dirty, not falsely clean.
  TogglePayload second{live};
  assert(owner.commitPrepared(UndoKind::Pattern, second, [&] { live = 43; }));
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 42);
  GroovePuterState::markSceneSaveSucceeded();
  assert(owner.togglePrepared<TogglePayload>(UndoKind::Pattern,
      [](const TogglePayload&) { return true; }, exchange) == UndoResult::Restored);
  assert(live == 43);
  assert(GroovePuterState::sceneDirty());

  static_assert(sizeof(UndoOwner) <= 1552,
      "one-slot toggle must not grow authoritative owner DRAM");
  std::printf("R9 one-slot toggle + fork invalidation owner=%zu bytes PASS\n",
              sizeof(UndoOwner));
  return 0;
}
