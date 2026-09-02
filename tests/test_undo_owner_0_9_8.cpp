#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/state/undo_owner.h"

namespace {

struct TinyPayload {
  int value{0};
};

struct OversizedPayload {
  uint8_t bytes[GroovePuterUndo::kUndoPayloadBytes + 1]{};
};

void resetState(uint32_t current, uint32_t persisted) {
  GroovePuterUndo::undoOwner().clear();
  GroovePuterState::restoreSceneRevision({current, persisted});
}

}  // namespace

int main() {
  using GroovePuterUndo::UndoKind;
  using GroovePuterUndo::UndoResult;
  auto& owner = GroovePuterUndo::undoOwner();

  static_assert(GroovePuterUndo::UndoOwner::payloadCapacity() == 1536,
                "R2 measured payload capacity changed");
  static_assert(sizeof(GroovePuterUndo::UndoOwner) <= 1552,
                "R2 authoritative owner DRAM budget changed");

  // Empty history is a no-op.
  resetState(1, 1);
  bool restored = false;
  auto result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return true; },
      [&](const TinyPayload&) { restored = true; });
  assert(result == UndoResult::NothingToUndo);
  assert(!restored);

  // Clean -> edit -> Undo returns to the exact clean pre-edit revision.
  resetState(10, 10);
  int model = 7;
  const TinyPayload clean_before{model};
  assert(owner.commitPrepared(UndoKind::Pattern, clean_before, [&] { model = 9; }));
  assert(model == 9);
  assert(owner.hasUndo());
  auto revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 11 && revision.persistedRevision == 10);
  assert(owner.committedRevision() == 11);

  result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return true; },
      [&](const TinyPayload& before) { model = before.value; });
  assert(result == UndoResult::Restored);
  assert(model == 7);
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 10 && revision.persistedRevision == 10);
  assert(!revision.dirty());
  assert(!owner.hasUndo());

  // Dirty -> edit -> Undo restores the previous dirty state, not false-clean.
  resetState(20, 19);
  model = 3;
  assert(owner.commitPrepared(UndoKind::Pattern, TinyPayload{3}, [&] { model = 4; }));
  result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return true; },
      [&](const TinyPayload& before) { model = before.value; });
  assert(result == UndoResult::Restored && model == 3);
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 20 && revision.persistedRevision == 19);
  assert(revision.dirty());

  // Save after the edit does not invalidate Undo. Undo restores older data but
  // must remain dirty relative to the newer saved edited state.
  resetState(30, 30);
  model = 100;
  assert(owner.commitPrepared(UndoKind::Pattern, TinyPayload{100}, [&] { model = 101; }));
  assert(owner.committedRevision() == 31);
  GroovePuterState::markSceneSaveSucceeded();
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 31 && revision.persistedRevision == 31);

  result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return true; },
      [&](const TinyPayload& before) { model = before.value; });
  assert(result == UndoResult::Restored && model == 100);
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 30 && revision.persistedRevision == 31);
  assert(revision.dirty());

  // A later persistent mutation expires the old receipt instead of reverting
  // across a newer committed edit that was not captured by this owner.
  resetState(40, 40);
  model = 1;
  assert(owner.commitPrepared(UndoKind::Pattern, TinyPayload{1}, [&] { model = 2; }));
  GroovePuterState::markSceneMutated();
  result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return true; },
      [&](const TinyPayload& before) { model = before.value; });
  assert(result == UndoResult::Expired);
  assert(model == 2);
  assert(!owner.hasUndo());

  // A temporarily unavailable target preserves the receipt. This is needed for
  // paged Pattern storage: Undo itself must not load files or pages.
  resetState(50, 50);
  model = 5;
  assert(owner.commitPrepared(UndoKind::Pattern, TinyPayload{5}, [&] { model = 6; }));
  result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return false; },
      [&](const TinyPayload& before) { model = before.value; });
  assert(result == UndoResult::TargetUnavailable);
  assert(owner.hasUndo() && model == 6);
  result = owner.undoPrepared<TinyPayload>(
      UndoKind::Pattern,
      [](const TinyPayload&) { return true; },
      [&](const TinyPayload& before) { model = before.value; });
  assert(result == UndoResult::Restored && model == 5);

  // Failed admission must not apply a mutation or destroy the previous Undo.
  resetState(60, 60);
  model = 8;
  assert(owner.commitPrepared(UndoKind::Pattern, TinyPayload{8}, [&] { model = 9; }));
  const uint32_t previous_committed_revision = owner.committedRevision();
  bool applied = false;
  OversizedPayload oversized{};
  assert(!owner.commitPrepared(UndoKind::Song, oversized, [&] { applied = true; }));
  assert(!applied);
  assert(owner.hasUndo());
  assert(owner.kind() == UndoKind::Pattern);
  assert(owner.committedRevision() == previous_committed_revision);

  assert(!owner.commitPrepared(UndoKind::None, TinyPayload{9}, [&] { applied = true; }));
  assert(!applied);
  assert(owner.hasUndo() && owner.kind() == UndoKind::Pattern);

  // A second successful mutation replaces the one-level receipt and advances
  // revision once for that logical commit.
  model = 9;
  const auto before_second = GroovePuterState::sceneRevisionSnapshot();
  assert(owner.commitPrepared(UndoKind::Song, TinyPayload{9}, [&] { model = 10; }));
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == before_second.currentRevision + 1);
  assert(owner.kind() == UndoKind::Song);
  TinyPayload read_back{};
  assert(owner.read(UndoKind::Song, read_back));
  assert(read_back.value == 9);

  std::printf("UndoOwner size=%zu payload=%zu\n",
              sizeof(GroovePuterUndo::UndoOwner),
              GroovePuterUndo::UndoOwner::payloadCapacity());
  std::puts("0.9.8 R2 Undo owner semantics: PASS");
  return 0;
}
