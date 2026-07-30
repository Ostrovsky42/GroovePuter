#include "../platform_sdl/arduino_compat.h"
#include "../scenes.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

SerialMock Serial;
SDMock SD;

namespace {

bool near(float actual, float expected, float epsilon = 0.0001f) {
  return std::fabs(actual - expected) <= epsilon;
}

void populateNonDefaultScene(SceneManager& manager) {
  manager.loadDefaultScene();
  manager.setBpm(137.5f);
  manager.setActiveSongSlot(1);
  manager.setTrackVolume(static_cast<int>(VoiceId::SynthA), 0.37f);

  Scene& scene = manager.currentScene();
  scene.feel.gridSteps = 32;
  scene.feel.timebase = 2;
  scene.feel.patternBars = 4;
  scene.feel.swingPct = 63;
  scene.feel.swingMask = 0x0355;
  scene.feel.lofiEnabled = true;
  scene.feel.lofiAmount = 74;
  scene.feel.driveEnabled = true;
  scene.feel.driveAmount = 81;
  scene.feel.tapeEnabled = true;

  scene.generatorParams.minNotes = 3;
  scene.generatorParams.maxNotes = 11;
  scene.generatorParams.minOctave = 29;
  scene.generatorParams.maxOctave = 71;
  scene.generatorParams.swingAmount = 0.41f;
  scene.generatorParams.velocityRange = 0.58f;
  scene.generatorParams.ghostNoteProbability = 0.27f;
  scene.generatorParams.microTimingAmount = 0.36f;
  scene.generatorParams.preferDownbeats = true;
  scene.generatorParams.scaleQuantize = false;
  scene.generatorParams.scaleRoot = 7;
  scene.generatorParams.scale = MIXOLYDIAN;

  DrumStep& drum =
      scene.drumBanks[0].patterns[0].voices[0].steps[3];
  drum.hit = true;
  drum.accent = true;
  drum.velocity = 47;
  drum.timing = -11;
  drum.probability = 83;
  drum.fx = 2;
  drum.fxParam = 5;

  SynthStep& synth = scene.synthABanks[0].patterns[0].steps[5];
  synth.note = 61;
  synth.slide = true;
  synth.accent = true;
  synth.ghost = true;
  synth.velocity = 72;
  synth.timing = 13;
  synth.probability = 76;
  synth.fx = 1;
  synth.fxParam = 4;

  std::strncpy(scene.customPhrases[0], "line one\nline\ttwo",
               Scene::kMaxPhraseLength - 1);
  scene.customPhrases[0][Scene::kMaxPhraseLength - 1] = '\0';
}

void destroyRoundTripFields(SceneManager& manager) {
  manager.setBpm(60.0f);
  manager.setActiveSongSlot(0);
  manager.setTrackVolume(static_cast<int>(VoiceId::SynthA), 1.0f);

  Scene& scene = manager.currentScene();
  scene.feel = FeelSettings();
  scene.generatorParams = GeneratorParams();
  scene.drumBanks[0].patterns[0].voices[0].steps[3] = DrumStep();
  scene.synthABanks[0].patterns[0].steps[5] = SynthStep();
  scene.customPhrases[0][0] = '\0';
}

void verifyRoundTrip(const SceneManager& manager) {
  const Scene& scene = manager.currentScene();
  assert(near(manager.getBpm(), 137.5f));
  assert(manager.activeSongSlot() == 1);
  assert(near(manager.getTrackVolume(static_cast<int>(VoiceId::SynthA)),
              0.37f));

  assert(scene.feel.gridSteps == 32);
  assert(scene.feel.timebase == 2);
  assert(scene.feel.patternBars == 4);
  assert(scene.feel.swingPct == 63);
  assert(scene.feel.swingMask == 0x0355);
  assert(scene.feel.lofiEnabled);
  assert(scene.feel.lofiAmount == 74);
  assert(scene.feel.driveEnabled);
  assert(scene.feel.driveAmount == 81);
  assert(scene.feel.tapeEnabled);

  assert(scene.generatorParams.minNotes == 3);
  assert(scene.generatorParams.maxNotes == 11);
  assert(scene.generatorParams.minOctave == 29);
  assert(scene.generatorParams.maxOctave == 71);
  assert(near(scene.generatorParams.swingAmount, 0.41f));
  assert(near(scene.generatorParams.velocityRange, 0.58f));
  assert(near(scene.generatorParams.ghostNoteProbability, 0.27f));
  assert(near(scene.generatorParams.microTimingAmount, 0.36f));
  assert(scene.generatorParams.preferDownbeats);
  assert(!scene.generatorParams.scaleQuantize);
  assert(scene.generatorParams.scaleRoot == 7);
  assert(scene.generatorParams.scale == MIXOLYDIAN);

  const DrumStep& drum =
      scene.drumBanks[0].patterns[0].voices[0].steps[3];
  assert(drum.hit);
  assert(drum.accent);
  assert(drum.velocity == 47);
  assert(drum.timing == -11);
  assert(drum.probability == 83);
  assert(drum.fx == 2);
  assert(drum.fxParam == 5);

  const SynthStep& synth = scene.synthABanks[0].patterns[0].steps[5];
  assert(synth.note == 61);
  assert(synth.slide);
  assert(synth.accent);
  assert(synth.ghost);
  assert(synth.velocity == 72);
  assert(synth.timing == 13);
  assert(synth.probability == 76);
  assert(synth.fx == 1);
  assert(synth.fxParam == 4);

  assert(std::string(scene.customPhrases[0]) == "line one\nline\ttwo");
}

}  // namespace

int main() {
  SceneManager manager;
  populateNonDefaultScene(manager);

  const std::string json = manager.dumpCurrentScene();
  assert(!json.empty());
  assert(json.find("\\n") != std::string::npos);
  assert(json.find("\\t") != std::string::npos);
  assert(json.find("\"vel\":[") != std::string::npos);
  assert(json.find("\"tim\":[") != std::string::npos);
  assert(json.find("\"ghost\":true") != std::string::npos);
  assert(json.find("\"swing\":63") != std::string::npos);
  assert(json.find("\"mask\":853") != std::string::npos);

  destroyRoundTripFields(manager);
  assert(manager.loadScene(json));
  verifyRoundTrip(manager);
  return 0;
}
