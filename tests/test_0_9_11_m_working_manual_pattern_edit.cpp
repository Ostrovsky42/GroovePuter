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

}  // namespace

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

  const int page = engine.currentPageIndex();
  const int bank = engine.current303BankIndex(0);
  const int pattern = engine.display303LocalPatternIndex(0);
  assert(page == 0);
  assert(bank == 0);
  assert(pattern == 0);

  MaterialReference current{};
  assert(engine.current303MaterialReference_(0, current));
  assert(current.address.voice == 0);
  assert(current.address.globalSlot == 0);
  assert(current.id == MaterialId{101});

  const bool changed = engine.adjustWorking303StepNote(0, 0, 1);
  assert(changed);

  // ACCEPTED A stays persistent truth until an explicit acceptance operation.
  assert(accepted.steps[0].note == 60);
  assert(sameStep(accepted.steps[3], untouched));

  // WORKING B owns the session edit and is bound to stable material identity,
  // not merely to its current page/bank/pattern coordinates.
  assert(engine.workingMaterial_[0].holdsPattern());
  assert(engine.workingMaterial_[0].patternMatches(current));
  const SynthPattern& working = engine.workingMaterial_[0].pattern();
  assert(working.steps[0].note == 61);
  assert(sameStep(working.steps[3], untouched));

  const SynthPattern* exposedWorking = engine.currentWorking303Pattern(0);
  assert(exposedWorking != nullptr);
  assert(exposedWorking->steps[0].note == 61);
  assert(sameStep(exposedWorking->steps[3], untouched));

  // The actual Pattern audio bank must hear B without reading mutable Scene.
  const auto* event =
      engine.patternRuntimeBank_.select(0, bank, pattern).eventForSourceStep(0);
  assert(event != nullptr);
  assert(event->note == 61);

  // Per-voice ownership: editing Synth A may not allocate or overwrite Synth B.
  assert(engine.workingMaterial_[1].empty());

  std::printf("M-WORKING identity-bound manual Pattern note edit: PASS\n");
  return 0;
}
