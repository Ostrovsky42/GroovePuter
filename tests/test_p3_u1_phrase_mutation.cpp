#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "src/state/runtime_phrase_edit.h"

namespace {

using GroovePuterPhraseEdit::MutationResult;
using GroovePuterPhraseEdit::RuntimePhraseUndoPayload;

PhraseRuntime::RuntimeSynthEventBuffer makePhrase(uint8_t bars = 2) {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = static_cast<uint16_t>(bars * PhraseRuntime::kTicksPerBar);
  phrase.count = 1;
  phrase.events[0].startTick = 360;
  phrase.events[0].durationSubticks =
      96 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;
  return phrase;
}

bool samePhrase(const PhraseRuntime::RuntimeSynthEventBuffer& lhs,
                const PhraseRuntime::RuntimeSynthEventBuffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

void resetOwners() {
  GroovePuterUndo::undoOwner().clear();
  GroovePuterState::restoreSceneRevision({17u, 17u});
}

void testCommitUndoRedoIsAtomicAndSessionOnly() {
  resetOwners();
  auto live = makePhrase();
  const auto before = live;
  const auto sceneBefore = GroovePuterState::sceneRevisionSnapshot();

  const MutationResult result = GroovePuterPhraseEdit::commit(
      0, live, [](PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
        candidate.events[0].durationSubticks =
            120 * PhraseRuntime::kSubticksPerTick;
      });

  assert(result == MutationResult::Committed);
  assert(live.events[0].durationSubticks ==
         120 * PhraseRuntime::kSubticksPerTick);
  assert(GroovePuterUndo::undoOwner().hasUndo());
  assert(GroovePuterUndo::undoOwner().kind() ==
         GroovePuterUndo::UndoKind::RuntimePhrase);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision ==
         sceneBefore.currentRevision);
  assert(GroovePuterState::sceneRevisionSnapshot().persistedRevision ==
         sceneBefore.persistedRevision);

  const auto changed = live;
  assert(GroovePuterPhraseEdit::toggleUndo(0, live) ==
         GroovePuterUndo::UndoResult::Restored);
  assert(samePhrase(live, before));
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision ==
         sceneBefore.currentRevision);
  assert(GroovePuterState::sceneRevisionSnapshot().persistedRevision ==
         sceneBefore.persistedRevision);

  assert(GroovePuterPhraseEdit::toggleUndo(0, live) ==
         GroovePuterUndo::UndoResult::Restored);
  assert(samePhrase(live, changed));
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision ==
         sceneBefore.currentRevision);
  assert(GroovePuterState::sceneRevisionSnapshot().persistedRevision ==
         sceneBefore.persistedRevision);
}

void testRejectedMutationPreservesLiveAndPreviousUndo() {
  resetOwners();

  auto retainedTarget = makePhrase();
  assert(GroovePuterPhraseEdit::commit(
             0, retainedTarget,
             [](PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
               candidate.events[0].note = 61;
             }) == MutationResult::Committed);

  RuntimePhraseUndoPayload retained{};
  assert(GroovePuterUndo::undoOwner().read(
      GroovePuterUndo::UndoKind::RuntimePhrase, retained));
  assert(retained.synthIndex == 0);

  auto live = makePhrase(1);
  live.events[0].startTick = 360;
  live.events[0].durationSubticks =
      12 * PhraseRuntime::kSubticksPerTick;
  const auto before = live;

  const MutationResult rejected = GroovePuterPhraseEdit::commit(
      1, live, [](PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
        candidate.events[0].durationSubticks =
            48 * PhraseRuntime::kSubticksPerTick;
      });

  assert(rejected == MutationResult::Rejected);
  assert(samePhrase(live, before));

  RuntimePhraseUndoPayload afterRejected{};
  assert(GroovePuterUndo::undoOwner().read(
      GroovePuterUndo::UndoKind::RuntimePhrase, afterRejected));
  assert(afterRejected.synthIndex == 0);
}

void testNoOpDoesNotConsumePreviousUndo() {
  resetOwners();
  auto first = makePhrase();
  assert(GroovePuterPhraseEdit::commit(
             0, first,
             [](PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
               candidate.events[0].velocity = 99;
             }) == MutationResult::Committed);

  RuntimePhraseUndoPayload retained{};
  assert(GroovePuterUndo::undoOwner().read(
      GroovePuterUndo::UndoKind::RuntimePhrase, retained));

  auto second = makePhrase();
  const MutationResult noChange = GroovePuterPhraseEdit::commit(
      1, second, [](PhraseRuntime::RuntimeSynthEventBuffer&) {});
  assert(noChange == MutationResult::NoChange);

  RuntimePhraseUndoPayload stillRetained{};
  assert(GroovePuterUndo::undoOwner().read(
      GroovePuterUndo::UndoKind::RuntimePhrase, stillRetained));
  assert(stillRetained.synthIndex == retained.synthIndex);
}

}  // namespace

int main() {
  static_assert(sizeof(RuntimePhraseUndoPayload) <=
                    GroovePuterUndo::UndoOwner::payloadCapacity(),
                "runtime Phrase before-image must fit the measured Undo slot");
  testCommitUndoRedoIsAtomicAndSessionOnly();
  testRejectedMutationPreservesLiveAndPreviousUndo();
  testNoOpDoesNotConsumePreviousUndo();
  std::puts("P3-U1 bounded runtime phrase mutation: OK");
  return 0;
}
