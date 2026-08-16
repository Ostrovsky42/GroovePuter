#include "../src/generation/migration/quantized_generation_commit.h"
#include "../src/state/undo_owner.h"

#include <cassert>
#include <cstdio>
#include <type_traits>

int main() {
  using GroovePuterRhythm::QuantizedGenerationDetail::GenerationUndoPayload;
  using GroovePuterUndo::UndoKind;
  using GroovePuterUndo::UndoResult;

  static_assert(std::is_trivially_copyable<GenerationUndoPayload>::value);
  static_assert(sizeof(GenerationUndoPayload) <= GroovePuterUndo::kUndoPayloadBytes);
  assert(GroovePuterRhythm::quantizedGenerationUndoPayloadSize() ==
         sizeof(GenerationUndoPayload));
  assert(GroovePuterRhythm::quantizedGenerationUndoPayloadSize() <=
         GroovePuterUndo::UndoOwner::payloadCapacity());

  auto& owner = GroovePuterUndo::undoOwner();
  owner.clear();
  GroovePuterState::restoreSceneRevision({100, 100});

  GenerationUndoPayload before{};
  before.target.page = 2;
  before.target.synthBank[0] = 0;
  before.target.synthBank[1] = 1;
  before.target.synthSlot[0] = 3;
  before.target.synthSlot[1] = 4;
  before.target.drumBank = 1;
  before.target.drumSlot = 5;
  before.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  before.genre.recipe = kDustyJazzRecipeId;
  before.mode = GrooveboxMode::Minimal;
  before.bpm = 74.0f;
  before.swingPct = 57;
  before.scope = GroovePuterRhythm::QuantizedGenerationScope::Full;
  before.synth[0].steps[0].note = 41;
  before.synth[1].steps[7].note = 62;
  before.drums.voices[0].steps[0].hit = true;

  int applied = 0;
  assert(owner.commitPrepared(UndoKind::Generation, before, [&] { applied = 1; }));
  assert(applied == 1);
  assert(owner.hasUndo());
  assert(owner.kind() == UndoKind::Generation);
  auto revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 101);
  assert(revision.persistedRevision == 100);
  assert(owner.committedRevision() == 101);

  GenerationUndoPayload readBack{};
  assert(owner.read(UndoKind::Generation, readBack));
  assert(readBack.target.page == 2);
  assert(readBack.genre.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi));
  assert(readBack.genre.recipe == kDustyJazzRecipeId);
  assert(readBack.bpm == 74.0f);
  assert(readBack.swingPct == 57);
  assert(readBack.synth[0].steps[0].note == 41);
  assert(readBack.synth[1].steps[7].note == 62);
  assert(readBack.drums.voices[0].steps[0].hit);

  bool restored = false;
  const UndoResult result = owner.undoPrepared<GenerationUndoPayload>(
      UndoKind::Generation,
      [](const GenerationUndoPayload& receipt) {
        return receipt.target.page == 2 &&
               receipt.genre.generativeMode ==
                   static_cast<uint8_t>(GenerativeMode::LoFi);
      },
      [&](const GenerationUndoPayload& receipt) {
        restored = receipt.genre.recipe == kDustyJazzRecipeId &&
                   receipt.synth[0].steps[0].note == 41;
      });
  assert(result == UndoResult::Restored);
  assert(restored);
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 100);
  assert(revision.persistedRevision == 100);
  assert(!owner.hasUndo());

  std::printf("0.9.9-B generation Undo payload=%zu/%zu bytes\n",
              sizeof(GenerationUndoPayload),
              GroovePuterUndo::UndoOwner::payloadCapacity());
  return 0;
}
