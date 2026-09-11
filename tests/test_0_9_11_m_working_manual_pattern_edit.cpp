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
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();

  SynthPattern& accepted =
      engine.sceneManager().currentScene().synthABanks[0].patterns[0];
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
  const int pattern = engine.songPatternIndexForTrack(SongTrack::SynthA);
  assert(page == 0);
  assert(bank == 0);
  assert(pattern == 0);

  const bool changed = engine.adjustWorking303StepNote(0, 0, 1);
  assert(changed);

  // ACCEPTED A stays persistent truth until an explicit acceptance operation.
  assert(accepted.steps[0].note == 60);
  assert(sameStep(accepted.steps[3], untouched));

  // WORKING B owns the session edit and remains bound to the exact target.
  assert(engine.workingMaterial_[0].holdsPattern());
  assert(engine.workingMaterial_[0].patternMatches(page, bank, pattern));
  const SynthPattern& working = engine.workingMaterial_[0].pattern();
  assert(working.steps[0].note == 61);
  assert(sameStep(working.steps[3], untouched));

  // This slice exposes Working explicitly; the broader active/migration read
  // path remains a separate MW-H checkpoint so edit ownership is not coupled to
  // Pattern->Melody migration in the same production commit.
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

  std::printf("M-WORKING manual Pattern note edit transaction: PASS\n");
  return 0;
}
