// 0.9.14 mix C: fader defaults apply to NEW scenes only; loading existing scenes is unchanged.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"

SerialMock Serial;
SDMock SD;

namespace {

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__,  \
                   #cond);                                                   \
      std::abort();                                                          \
    }                                                                        \
  } while (0)

constexpr float kTestRate = 44100.0f;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void setAll(Scene& scene, float v) {
  for (float& x : scene.trackVolumes) x = v;
}

void expectMixC(const Scene& scene) {
  CHECK(near(scene.trackVolumes[(int)VoiceId::SynthA], 1.5f));
  CHECK(near(scene.trackVolumes[(int)VoiceId::SynthB], 1.5f));
  for (int i = (int)VoiceId::DrumKick; i < (int)VoiceId::Count; ++i) {
    CHECK(near(scene.trackVolumes[i], 0.45f));
  }
}

void expectAll(const Scene& scene, float v) {
  for (int i = 0; i < (int)VoiceId::Count; ++i) CHECK(near(scene.trackVolumes[i], v));
}

std::string withoutTrackVolumes(std::string json) {
  const std::string key = "\"trackVolumes\":[";
  const size_t at = json.find(key);
  CHECK(at != std::string::npos);
  const size_t end = json.find(']', at);
  CHECK(end != std::string::npos);
  size_t from = at;
  size_t to = end + 1;
  if (to < json.size() && json[to] == ',') ++to;   // key is followed by a comma, or preceded by one
  else if (from > 0 && json[from - 1] == ',') --from;
  json.erase(from, to - from);
  CHECK(json.find("trackVolumes") == std::string::npos);
  return json;
}

}  // namespace

int main() {
  SceneStorageSdl storage;
  CHECK(PatternPagingService::setProjectName("mix-defaults"));
  CHECK(PatternPagingService::clearProjectPages());
  MiniAcid engine{kTestRate, &storage};
  engine.init();
  SceneManager& scenes = engine.sceneManager();

  // New scene paths: wipeToZero (new scene, clear project), loadDefaultScene (reset, boot fallback).
  setAll(scenes.currentScene(), 0.3f);
  CHECK(engine.createNewSceneWithName("mix-new-scene"));
  expectMixC(scenes.currentScene());

  setAll(scenes.currentScene(), 0.3f);
  scenes.wipeToZero();
  expectMixC(scenes.currentScene());

  setAll(scenes.currentScene(), 0.3f);
  scenes.loadDefaultScene();
  expectMixC(scenes.currentScene());
  std::puts("mix C: new scene, clear project and default scene get synth 1.5 / drums 0.45: PASS");

  // A saved scene keeps its own faders (persisted, not re-defaulted).
  scenes.wipeToZero();
  scenes.currentScene().trackVolumes[(int)VoiceId::DrumKick] = 0.7f;
  const std::string saved = scenes.dumpCurrentScene();
  setAll(scenes.currentScene(), 0.3f);
  CHECK(scenes.loadScene(saved));
  CHECK(near(scenes.currentScene().trackVolumes[(int)VoiceId::SynthA], 1.5f));
  CHECK(near(scenes.currentScene().trackVolumes[(int)VoiceId::DrumKick], 0.7f));
  CHECK(near(scenes.currentScene().trackVolumes[(int)VoiceId::DrumSnare], 0.45f));
  std::puts("mix C: a saved scene reloads with exactly its own faders: PASS");

  // A scene written before faders existed (no key) loads at 1.0 -- through both loaders.
  const std::string legacy = withoutTrackVolumes(saved);
  scenes.wipeToZero();
  expectMixC(scenes.currentScene());
  CHECK(scenes.loadScene(legacy));                 // production streaming loader
  expectAll(scenes.currentScene(), 1.0f);
  scenes.wipeToZero();
  CHECK(scenes.loadSceneJson(legacy));             // document loader
  expectAll(scenes.currentScene(), 1.0f);
  std::puts("mix C: scene files without trackVolumes still load at 1.0 (streaming and document): PASS");

  std::puts("0.9.14 mix defaults: PASS");
  return 0;
}
