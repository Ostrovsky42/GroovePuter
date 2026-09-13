#include <cassert>
#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

namespace {
bool sameStep(const SynthStep& a, const SynthStep& b) {
  return a.note == b.note && a.slide == b.slide && a.accent == b.accent &&
         a.ghost == b.ghost && a.velocity == b.velocity &&
         a.timing == b.timing && a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}
}

int main() {
  using GroovePuterMaterial::MaterialId;
  using GroovePuterMaterial::MaterialReference;

  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  Scene& scene = engine.sceneManager().currentScene();
  scene.materialSlots[0][0].id = MaterialId{101};
  SynthPattern& accepted = scene.synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;
  accepted.steps[3].note = 61;
  accepted.steps[3].slide = true;
  accepted.steps[3].accent = true;
  accepted.steps[3].ghost = true;
  accepted.steps[3].velocity = 73;
  accepted.steps[3].timing = -7;
  accepted.steps[3].fx = static_cast<uint8_t>(StepFx::Reverse);
  accepted.steps[3].fxParam = 91;
  accepted.steps[3].probability = 47;
  const SynthStep untouched = accepted.steps[3];

  assert(engine.workingMaterial_[0].empty());
  assert(engine.workingMaterial_[1].empty());
  assert(engine.rebuildPatternRuntimeEventBank());

  MaterialReference current{};
  assert(engine.current303MaterialReference_(0, current));
  assert(current.id == MaterialId{101});
  assert(engine.adjustWorking303StepNote(0, 0, 1));

  assert(accepted.steps[0].note == 60);
  assert(sameStep(accepted.steps[3], untouched));
  assert(engine.workingMaterial_[0].holdsPattern());
  assert(engine.workingMaterial_[0].patternMatches(current));
  assert(engine.currentWorking303Pattern(0)->steps[0].note == 61);

  const int bank = engine.current303BankIndex(0);
  const int pattern = engine.display303LocalPatternIndex(0);
  const auto* event = engine.patternRuntimeBank_.select(0, bank, pattern).eventForSourceStep(0);
  assert(event != nullptr && event->note == 61);
  assert(engine.workingMaterial_[1].empty());

  std::puts("C1 manual Working prepare->publish->store: PASS");
  return 0;
}
