#include "../src/state/undo_owner.h"
#include "../src/state/undo_receipts.h"

#include <cassert>
#include <cstdio>

int main() {
  using GroovePuterUndo::SynthPatternUndoPayload;
  using GroovePuterUndo::UndoKind;
  using GroovePuterUndo::UndoResult;

  static_assert(sizeof(SynthPatternUndoPayload) == 116);
  static_assert(sizeof(SynthPatternUndoPayload) <=
                GroovePuterUndo::UndoOwner::payloadCapacity());

  auto& owner = GroovePuterUndo::undoOwner();
  owner.clear();
  GroovePuterState::restoreSceneRevision({41, 41});

  SynthPatternUndoPayload before{};
  before.pageIndex = 2;
  before.synthIndex = 1;
  before.bankIndex = 0;
  before.patternIndex = 3;
  before.before.steps[0].note = 36;
  before.before.steps[0].accent = true;
  before.before.steps[4].note = 43;
  before.before.steps[4].slide = true;
  before.before.steps[7].timing = -3;
  before.before.steps[11].probability = 72;

  SynthPattern generated = before.before;
  generated.steps[0].note = 38;
  generated.steps[0].accent = false;
  generated.steps[4].note = 50;
  generated.steps[4].slide = false;
  generated.steps[7].timing = 5;
  generated.steps[11].probability = 84;

  SynthPattern current = before.before;
  assert(owner.commitPrepared(
      UndoKind::Generation, before, [&] { current = generated; }));
  assert(owner.hasUndo());
  assert(owner.kind() == UndoKind::Generation);
  assert(owner.payloadSize() == sizeof(SynthPatternUndoPayload));
  assert(current.steps[0].note == 38);
  assert(current.steps[4].note == 50);
  assert(current.steps[7].timing == 5);
  assert(current.steps[11].probability == 84);

  auto revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 42);
  assert(revision.persistedRevision == 41);
  assert(owner.committedRevision() == 42);

  SynthPatternUndoPayload readBack{};
  assert(owner.read(UndoKind::Generation, readBack));
  assert(readBack.pageIndex == 2);
  assert(readBack.synthIndex == 1);
  assert(readBack.bankIndex == 0);
  assert(readBack.patternIndex == 3);
  assert(readBack.before.steps[0].note == 36);
  assert(readBack.before.steps[0].accent);
  assert(readBack.before.steps[4].slide);
  assert(readBack.before.steps[7].timing == -3);
  assert(readBack.before.steps[11].probability == 72);

  const UndoResult result = owner.undoPrepared<SynthPatternUndoPayload>(
      UndoKind::Generation,
      [](const SynthPatternUndoPayload& receipt) {
        return GroovePuterUndo::validSynthPatternAddress(receipt);
      },
      [&](const SynthPatternUndoPayload& receipt) {
        current = receipt.before;
      });
  assert(result == UndoResult::Restored);
  assert(current.steps[0].note == 36);
  assert(current.steps[0].accent);
  assert(current.steps[4].note == 43);
  assert(current.steps[4].slide);
  assert(current.steps[7].timing == -3);
  assert(current.steps[11].probability == 72);

  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 41);
  assert(revision.persistedRevision == 41);
  assert(!owner.hasUndo());

  std::printf("0.9.9-B2 Pattern generation receipt=%zu/%zu bytes\n",
              sizeof(SynthPatternUndoPayload),
              GroovePuterUndo::UndoOwner::payloadCapacity());
  return 0;
}
