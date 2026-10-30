// Ctrl+Z after the Alt+R copy removes it: the receipt's "before" is the
// no-Melody state and STEPS are unchanged.
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
  std::fprintf(stderr, "ALT-R-UNDO FAIL: %s\n", msg);
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

  expect(PhraseSourceToggle::switchSource(engine, nullptr, kA) ==
             PhraseSourceToggle::Result::MadePhrase,
         "copy failed");
  expect(engine.currentPhraseBuffer(kA).count == 2, "copy produced no notes");

  auto& owner = GroovePuterUndo::undoOwner();
  expect(owner.hasUndo() && owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase,
         "no RuntimePhrase receipt");
  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  expect(owner.read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt), "receipt unreadable");
  expect(receipt.before.count == 0, "receipt before is not the no-Melody state");
  expect(receipt.source == static_cast<uint8_t>(MiniAcid::SequencedSource::Pattern),
         "receipt source is not STEPS");

  // Ctrl+Z exactly as SynthSequencerPage does it.
  GroovePuterUndo::exchangeFixedValue(engine.currentPhraseBuffer(kA), receipt.before);
  engine.setSequencedSource(kA, static_cast<MiniAcid::SequencedSource>(receipt.source));
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern,
         "undo did not return to STEPS");
  expect(engine.currentPhraseBuffer(kA).count == 0, "undo left the copied notes");
  expect(std::memcmp(&engine.editSynthPattern(kA), &stepsBefore, sizeof(SynthPattern)) == 0,
         "STEPS changed");

  if (g_failures == 0) {
    std::printf("alt+r copy undo: PASS\n");
    return 0;
  }
  return 1;
}
