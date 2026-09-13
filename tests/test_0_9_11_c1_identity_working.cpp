#include <cassert>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

int main() {
  using GroovePuterMaterial::MaterialAddress;
  using GroovePuterMaterial::MaterialId;
  using GroovePuterMaterial::MaterialReference;

  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  Scene& scene = engine.sceneManager().currentScene();
  SynthPattern& accepted = scene.synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;

  constexpr MaterialAddress address{0, 0};
  constexpr MaterialId idM{101};
  constexpr MaterialId idN{102};
  constexpr MaterialReference refM{address, idM};
  constexpr MaterialReference refN{address, idN};
  scene.materialSlots[0][0].id = idM;

  assert(engine.workingMaterial_[0].empty());
  assert(!engine.hasModifiedWorking303Pattern(0));

  engine.workingMaterial_[0].storePattern(accepted, refM);
  assert(engine.currentWorking303Pattern(0) != nullptr);
  assert(!engine.hasModifiedWorking303Pattern(0));

  SynthPattern working = accepted;
  working.steps[0].note = 61;
  engine.workingMaterial_[0].storePattern(working, refM);
  assert(engine.hasModifiedWorking303Pattern(0));

  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  assert(engine.hasModifiedWorking303Pattern(0));
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
  assert(engine.hasModifiedWorking303Pattern(0));

  scene.materialSlots[0][0].id = idN;
  assert(!engine.hasModifiedWorking303Pattern(0));
  assert(engine.currentWorking303Pattern(0) == nullptr);

  engine.workingMaterial_[0].storePattern(working, refN);
  assert(engine.hasModifiedWorking303Pattern(0));
  assert(engine.currentWorking303Pattern(0) != nullptr);

  scene.materialSlots[0][0].id = MaterialId{};
  assert(!engine.hasModifiedWorking303Pattern(0));
  assert(engine.currentWorking303Pattern(0) == nullptr);

  std::puts("C1 identity-bound Working/ABA: PASS");
  return 0;
}
