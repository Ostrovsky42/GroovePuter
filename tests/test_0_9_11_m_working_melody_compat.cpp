#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/dsp/miniacid_engine.h"
#include "src/phrase/runtime_phrase_edit.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using GroovePuterMaterial::MaterialKind;

Buffer melody(uint8_t note) {
  Buffer out{};
  out.lengthTicks = PhraseRuntime::kTicksPerBar;
  out.count = 1;
  out.events[0] = {0,
                   static_cast<uint16_t>(24u * PhraseRuntime::kSubticksPerTick),
                   note,
                   100,
                   100,
                   0,
                   0,
                   0};
  return out;
}

void test_phrase_edit_uses_same_payload() {
  MiniAcid engine{44100.0f, nullptr};
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  engine.currentPhraseBuffer(0) = melody(60);

  Buffer prepared = engine.currentPhraseBuffer(0);
  assert(RuntimePhraseEdit::transposeEvent(prepared, 0, 1) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(RuntimePhraseEdit::commit(engine.currentPhraseBuffer(0), prepared));
  assert(engine.setPhraseLength(0, 2));

  const Buffer& live = engine.currentPhraseBuffer(0);
  assert(live.count == 1);
  assert(live.events[0].note == 61);
  assert(live.lengthTicks == PhraseRuntime::kTicksPerBar * 2u);
}

void test_runtime_phrase_undo_toggles_payload() {
  MiniAcid engine{44100.0f, nullptr};
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  const Buffer before = melody(64);
  const Buffer after = melody(67);
  engine.currentPhraseBuffer(0) = before;

  auto& owner = GroovePuterUndo::undoOwner();
  owner.clear();

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = 0;
  receipt.source = 1;
  receipt.before = before;
  assert(owner.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase,
      receipt,
      [&]() { engine.currentPhraseBuffer(0) = after; }));
  assert(engine.currentPhraseBuffer(0).events[0].note == 67);

  const auto undo =
      owner.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
          GroovePuterUndo::UndoKind::RuntimePhrase,
          [](const GroovePuterUndo::RuntimePhraseUndoPayload& value) {
            return GroovePuterUndo::validRuntimePhraseUndoPayload(value);
          },
          [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
            GroovePuterUndo::exchangeFixedValue(
                engine.currentPhraseBuffer(0), retained.before);
          });
  assert(undo == GroovePuterUndo::UndoResult::Restored);
  assert(engine.currentPhraseBuffer(0).events[0].note == 64);
  assert(owner.nextIsRedo());

  const auto redo =
      owner.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
          GroovePuterUndo::UndoKind::RuntimePhrase,
          [](const GroovePuterUndo::RuntimePhraseUndoPayload& value) {
            return GroovePuterUndo::validRuntimePhraseUndoPayload(value);
          },
          [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
            GroovePuterUndo::exchangeFixedValue(
                engine.currentPhraseBuffer(0), retained.before);
          });
  assert(redo == GroovePuterUndo::UndoResult::Restored);
  assert(engine.currentPhraseBuffer(0).events[0].note == 67);
  owner.clear();
}

void test_pending_activation_copies_before_publish() {
  MiniAcid engine{44100.0f, nullptr};
  engine.currentPhraseBuffer(0) = melody(50);
  const auto activeBefore = engine.activeMaterial(0);

  Buffer prepared = melody(69);
  assert(engine.stagePendingMaterial(0, 5, MaterialKind::Melody, &prepared));
  prepared.events[0].note = 72;  // caller-owned preparation may now change

  const auto activeAfterStage = engine.activeMaterial(0);
  assert(activeAfterStage.slot == activeBefore.slot);
  assert(activeAfterStage.kind == activeBefore.kind);
  assert(engine.currentPhraseBuffer(0).events[0].note == 50);

  engine.activatePendingMaterial();
  const auto activeAfterBoundary = engine.activeMaterial(0);
  assert(activeAfterBoundary.slot == 5);
  assert(activeAfterBoundary.kind == MaterialKind::Melody);
  assert(engine.currentPhraseBuffer(0).events[0].note == 69);
}

void test_failed_next_leaves_active_and_working_unchanged() {
  MiniAcid engine{44100.0f, nullptr};
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  engine.currentPhraseBuffer(0) = melody(74);
  const Buffer before = engine.currentPhraseBuffer(0);
  const auto activeBefore = engine.activeMaterial(0);

  assert(!engine.stagePendingMaterial(0, 9, MaterialKind::Melody, nullptr));

  const auto activeAfter = engine.activeMaterial(0);
  assert(activeAfter.slot == activeBefore.slot);
  assert(activeAfter.kind == activeBefore.kind);
  assert(std::memcmp(&before, &engine.currentPhraseBuffer(0), sizeof(Buffer)) == 0);
}

}  // namespace

int main() {
  test_phrase_edit_uses_same_payload();
  test_runtime_phrase_undo_toggles_payload();
  test_pending_activation_copies_before_publish();
  test_failed_next_leaves_active_and_working_unchanged();
  std::puts("M-WORKING Melody compatibility: PASS");
  return 0;
}
