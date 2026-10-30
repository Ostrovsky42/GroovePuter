// Alt+N from STEPS with no Melody builds the EMPTY Melody directly. The Undo
// receipt's "before" is the state with no Melody, so Ctrl+Z gives back STEPS and
// never a Melody copied from the steps.
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;
void expect(bool ok, const char* msg) {
  if (ok) return;
  std::fprintf(stderr, "ALT-N-UNDO FAIL: %s\n", msg);
  ++g_failures;
}
constexpr int kA = 0;
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};
  expect(engine.rebuildPatternRuntimeEventBank(), "runtime Pattern bank must build");
  engine.editSynthPattern(kA).steps[0].note = 36;
  engine.editSynthPattern(kA).steps[4].note = 43;
  const SynthPattern stepsBefore = engine.editSynthPattern(kA);
  expect(engine.retainedWorkingMelody(kA) == nullptr, "precondition: no Melody");

  expect(PhraseSourceToggle::newEmptyMelody(engine, nullptr, kA), "Alt+N path failed");
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase,
         "Alt+N did not land on MELODY");
  expect(engine.currentPhraseBuffer(kA).count == 0, "Alt+N Melody is not empty");
  expect(engine.currentPhraseBuffer(kA).lengthTicks > 0, "Alt+N Melody has no extent");

  // The retained receipt: "before" must not be a copy of the steps.
  auto& owner = GroovePuterUndo::undoOwner();
  expect(owner.hasUndo() && owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase,
         "no RuntimePhrase receipt");
  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  expect(owner.read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt), "receipt unreadable");
  expect(receipt.before.count == 0, "receipt before holds notes (a copy of the steps)");
  expect(receipt.source == static_cast<uint8_t>(MiniAcid::SequencedSource::Pattern),
         "receipt source is not STEPS");

  // Ctrl+Z exactly as SynthSequencerPage does it (exchange live <-> before).
  GroovePuterUndo::exchangeFixedValue(engine.currentPhraseBuffer(kA), receipt.before);
  engine.setSequencedSource(kA, static_cast<MiniAcid::SequencedSource>(receipt.source));
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern,
         "undo did not return to STEPS");
  expect(engine.currentPhraseBuffer(kA).count == 0, "undo restored notes into the Melody");
  // Known limit: undo exchanges the buffer only; the working-material slot stays
  // typed Melody, holding the clean empty "before" (no notes, source STEPS).
  expect(std::memcmp(&engine.editSynthPattern(kA), &stepsBefore, sizeof(SynthPattern)) == 0,
         "STEPS material changed");

  if (g_failures == 0) {
    std::printf("alt+n undo no-copy: PASS\n");
    return 0;
  }
  return 1;
}
