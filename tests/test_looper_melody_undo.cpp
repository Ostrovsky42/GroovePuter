// LOOPER > MELODY commit: SynthSequencerPage::replaceMelodyOneUndo() puts a Melody into
// the voice with ONE Undo receipt. From STEPS, Ctrl+Z returns to STEPS (not to a copy of
// the steps); from MELODY it returns the previous Melody.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"
#include "src/input/looper_melody_export.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/pages/synth_sequencer_page.h"

SerialMock Serial;
SDMock SD;

namespace {
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Done = SynthSequencerPage::ReplaceOneUndoResult;
constexpr int kA = 0;

Buffer fromLoop(uint8_t first, uint8_t second) {
  GroovePuterLooper::LoopLayer layer;
  layer.reset();
  assert(layer.noteOn(1, 0, first, 100, 0.0, 32) != GroovePuterLooper::LoopLayer::Write::Full);
  assert(layer.noteOff(1, 4.0, 32));
  assert(layer.noteOn(2, 1, second, 90, 8.0, 32) != GroovePuterLooper::LoopLayer::Write::Full);
  assert(layer.noteOff(2, 12.0, 32));
  Buffer out;
  assert(GroovePuterLooper::loopToMelody(layer, 32, 2, 0, out) ==
         GroovePuterLooper::MelodyExportResult::Ok);
  return out;
}

// Ctrl+Z exactly as SynthSequencerPage does it (exchange live <-> before, and the source).
void ctrlZ(MiniAcid& engine, int voice) {
  auto& owner = GroovePuterUndo::undoOwner();
  assert(owner.hasUndo() && owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase);
  const auto result = owner.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
      GroovePuterUndo::UndoKind::RuntimePhrase,
      [&](const GroovePuterUndo::RuntimePhraseUndoPayload& r) {
        return GroovePuterUndo::validRuntimePhraseUndoPayload(r) && r.voiceIndex == voice;
      },
      [&](GroovePuterUndo::RuntimePhraseUndoPayload& r) {
        auto& live = engine.currentPhraseBuffer(voice);
        GroovePuterUndo::exchangeFixedValue(live, r.before);
        const auto current = engine.currentSequencedSource(voice);
        engine.setSequencedSource(voice, static_cast<MiniAcid::SequencedSource>(r.source));
        r.source = static_cast<uint8_t>(current);
      });
  assert(result == GroovePuterUndo::UndoResult::Restored);
}
}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_looper_melody_undo";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("lp_melody", 0);
  PatternPagingService::setProjectName("lp_melody");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(44100.0f, &storage);
  engine.init();
  engine.setSongMode(false);
  GroovePuterUndo::undoOwner().clear();

  engine.editSynthPattern(kA).steps[0].note = 36;
  engine.editSynthPattern(kA).steps[4].note = 43;
  const SynthPattern stepsBefore = engine.editSynthPattern(kA);
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern);
  assert(engine.retainedWorkingMelody(kA) == nullptr);

  // 1. From STEPS with no Melody: one receipt, Ctrl+Z returns to STEPS.
  const Buffer loop1 = fromLoop(60, 67);
  assert(SynthSequencerPage::replaceMelodyOneUndo(engine, AudioGuard{}, kA, loop1) == Done::Done);
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase);
  assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(kA), loop1));
  assert(engine.hasUnsavedWorkingMelody(kA));  // not saved: Alt+Enter does that
  {
    GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
    assert(GroovePuterUndo::undoOwner().read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt));
    assert(receipt.source == static_cast<uint8_t>(MiniAcid::SequencedSource::Pattern));
    assert(receipt.before.count == 0);  // not a copy of the steps
  }
  ctrlZ(engine, kA);
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern);
  assert(engine.currentPhraseBuffer(kA).count == 0);
  assert(std::memcmp(&engine.editSynthPattern(kA), &stepsBefore, sizeof(SynthPattern)) == 0);
  ctrlZ(engine, kA);  // the same single step again = redo: the exported Melody comes back
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase);
  assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(kA), loop1));

  // 2. Already on MELODY: replaced, Ctrl+Z returns the previous Melody.
  GroovePuterUndo::undoOwner().clear();
  const Buffer loop2 = fromLoop(50, 55);
  assert(SynthSequencerPage::replaceMelodyOneUndo(engine, AudioGuard{}, kA, loop2) == Done::Done);
  assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(kA), loop2));
  ctrlZ(engine, kA);
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase);
  assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(kA), loop1));

  // 3. Back on STEPS with a retained Melody: replaced and shown; Ctrl+Z returns the
  //    retained Melody and STEPS.
  GroovePuterUndo::undoOwner().clear();
  engine.setSequencedSource(kA, MiniAcid::SequencedSource::Pattern);
  assert(engine.retainedWorkingMelody(kA) != nullptr);
  assert(SynthSequencerPage::replaceMelodyOneUndo(engine, AudioGuard{}, kA, loop2) == Done::Done);
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase);
  assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(kA), loop2));
  ctrlZ(engine, kA);
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern);
  assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(kA), loop1));

  // 4. An invalid Melody is refused and changes nothing.
  GroovePuterUndo::undoOwner().clear();
  Buffer bad = loop1;
  bad.lengthTicks = 100;
  assert(SynthSequencerPage::replaceMelodyOneUndo(engine, AudioGuard{}, kA, bad) == Done::Rejected);
  assert(!GroovePuterUndo::undoOwner().hasUndo());
  assert(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern);

  // 5. For the report: what the GRAB path (replaceMelodyFor from STEPS) leaves in the receipt.
  MiniAcid& other = engine;
  constexpr int kB = 1;
  GroovePuterUndo::undoOwner().clear();
  other.editSynthPattern(kB).steps[0].note = 36;
  other.editSynthPattern(kB).steps[4].note = 43;
  assert(SynthSequencerPage::replaceMelodyFor(other, AudioGuard{}, kB, loop1));
  GroovePuterUndo::RuntimePhraseUndoPayload grab{};
  assert(GroovePuterUndo::undoOwner().read(GroovePuterUndo::UndoKind::RuntimePhrase, grab));
  std::printf("[info] replaceMelodyFor from STEPS: receipt source=%u before.count=%u "
              "(source Pattern + count 0 would be the clean STEPS receipt)\n",
              static_cast<unsigned>(grab.source), static_cast<unsigned>(grab.before.count));

  std::puts("looper melody undo: PASS");
  return 0;
}
