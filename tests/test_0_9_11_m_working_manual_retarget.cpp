#include <cassert>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

int main() {
  using GroovePuterMaterial::MaterialId;

  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  Scene& scene = engine.sceneManager().currentScene();

  scene.materialSlots[0][0].id = MaterialId{101};
  scene.materialSlots[0][1].id = MaterialId{102};
  scene.materialSlots[1][0].id = MaterialId{201};
  scene.materialSlots[1][1].id = MaterialId{202};
  scene.synthABanks[0].patterns[0].steps[0].note = 60;

  assert(engine.currentPageIndex() == 0);
  assert(engine.current303BankIndex(0) == 0);
  assert(engine.display303LocalPatternIndex(0) == 0);
  assert(engine.adjustWorking303StepNote(0, 0, 1));
  assert(engine.hasModifiedWorking303Pattern(0));

  // A modified Working value may not be orphaned by a different manual target.
  assert(!engine.tryManual303TargetSwitch(0, 0, 1));
  assert(engine.current303BankIndex(0) == 0);
  assert(engine.display303LocalPatternIndex(0) == 0);
  assert(engine.hasModifiedWorking303Pattern(0));
  assert(scene.synthABanks[0].patterns[0].steps[0].note == 60);

  // Same target is a no-op success, not a false refusal.
  assert(engine.tryManual303TargetSwitch(0, 0, 0));
  assert(engine.hasModifiedWorking303Pattern(0));

  // Playback representation is unrelated to edit-target ownership.
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  assert(!engine.tryManual303TargetSwitch(0, 0, 1));
  assert(engine.display303LocalPatternIndex(0) == 0);
  assert(engine.hasModifiedWorking303Pattern(0));

  // Per-voice ownership: dirty Synth A must not block clean Synth B.
  assert(engine.tryManual303TargetSwitch(1, 0, 1));
  assert(engine.display303LocalPatternIndex(1) == 1);

  // A page switch changes both voices' resident edit targets and therefore must
  // be refused while either voice owns modified Working.
  assert(!engine.tryManualPageSwitch(1));
  assert(engine.currentPageIndex() == 0);
  assert(engine.targetPageIndex() == -1);

  MiniAcid clean{44100.0f, nullptr};
  clean.sceneManager().loadDefaultScene();
  clean.sceneManager().currentScene().materialSlots[0][0].id = MaterialId{301};
  clean.sceneManager().currentScene().materialSlots[0][1].id = MaterialId{302};
  assert(clean.tryManual303TargetSwitch(0, 0, 1));
  assert(clean.display303LocalPatternIndex(0) == 1);
  assert(clean.tryManualPageSwitch(1));
  assert(clean.targetPageIndex() == 1);

  std::puts("M-WORKING MW-L manual target-switch guard: PASS");
  return 0;
}
