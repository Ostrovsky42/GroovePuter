#include <cassert>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

int main() {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();

  SynthPattern& accepted =
      engine.sceneManager().currentScene().synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;
  accepted.steps[4].note = 64;
  accepted.steps[4].accent = false;

  const int page = engine.currentPageIndex();
  const int bank = engine.current303BankIndex(0);
  const int pattern = engine.display303LocalPatternIndex(0);
  assert(page == 0);
  assert(bank == 0);
  assert(pattern == 0);

  // EMPTY has no session delta.
  assert(engine.workingMaterial_[0].empty());
  assert(!engine.hasModifiedWorking303Pattern(0));

  // A byte-for-musical-byte Working copy is not modified merely because a
  // session payload exists.
  engine.workingMaterial_[0].storePattern(accepted, page, bank, pattern);
  assert(!engine.hasModifiedWorking303Pattern(0));

  // A note delta is modified.
  SynthPattern working = accepted;
  working.steps[0].note = 61;
  engine.workingMaterial_[0].storePattern(working, page, bank, pattern);
  assert(engine.hasModifiedWorking303Pattern(0));

  // The predicate is semantic, not note-only: non-note musical fields count.
  working = accepted;
  working.steps[4].accent = true;
  engine.workingMaterial_[0].storePattern(working, page, bank, pattern);
  assert(engine.hasModifiedWorking303Pattern(0));

  // A retained Working Pattern for another target must not dirty this target.
  engine.workingMaterial_[0].storePattern(working, page, bank, pattern + 1);
  assert(!engine.hasModifiedWorking303Pattern(0));

  // Playback source is not physical Working lifetime. A Pattern delta remains
  // modified across Pattern -> Phrase -> Pattern source toggles.
  engine.workingMaterial_[0].storePattern(working, page, bank, pattern);
  assert(engine.hasModifiedWorking303Pattern(0));
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  assert(engine.hasModifiedWorking303Pattern(0));
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
  assert(engine.hasModifiedWorking303Pattern(0));

  // Melody is a different physical representation and is not a modified
  // Working Pattern.
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  engine.workingMaterial_[0].storeMelody(melody);
  assert(!engine.hasModifiedWorking303Pattern(0));

  // Per-voice ownership remains independent.
  assert(!engine.hasModifiedWorking303Pattern(1));

  std::printf("M-WORKING derived Pattern modified-state: PASS\n");
  return 0;
}
