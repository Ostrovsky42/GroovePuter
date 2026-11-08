// Per-cell source override persistence: bits survive save and load through both
// loaders, a file without the keys loads with all bits 0, and the writer emits
// no key when every bit is 0.
#include <cassert>
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
bool anyBit(const Scene& scene) {
  for (int s = 0; s < 2; ++s)
    if (scene.songs[s].anyStepsOverride()) return true;
  return false;
}
}  // namespace

int main() {
  SceneManager scenes;
  scenes.loadDefaultScene();
  Scene& scene = scenes.currentScene();
  scene.songs[0].length = 6;
  scene.songs[0].setStepsOverride(0, 1, true);
  scene.songs[0].setStepsOverride(1, 4, true);
  scene.songs[1].length = 3;
  scene.songs[1].setStepsOverride(0, 2, true);

  const std::string withBits = scenes.dumpCurrentScene();
  assert(withBits.find("\"as\":1") != std::string::npos);
  assert(withBits.find("\"bs\":1") != std::string::npos);

  for (int loader = 0; loader < 2; ++loader) {  // streaming, then document
    scenes.wipeToZero();
    assert(!anyBit(scenes.currentScene()));
    assert(loader == 0 ? scenes.loadScene(withBits) : scenes.loadSceneJson(withBits));
    const Scene& back = scenes.currentScene();
    for (int s = 0; s < 2; ++s) {
      for (int v = 0; v < 2; ++v) {
        for (int r = 0; r < Song::kMaxPositions; ++r) {
          const bool want = (s == 0 && v == 0 && r == 1) ||
                            (s == 0 && v == 1 && r == 4) ||
                            (s == 1 && v == 0 && r == 2);
          assert(back.songs[s].stepsOverrideAt(v, r) == want);
        }
      }
    }
  }
  std::puts("bits survive save and load (streaming and document): PASS");

  // Writer emits nothing when all bits are 0.
  scenes.wipeToZero();
  const std::string clean = scenes.dumpCurrentScene();
  assert(clean.find("\"as\"") == std::string::npos);
  assert(clean.find("\"bs\"") == std::string::npos);

  // An old file (keys stripped) loads with every bit 0 and the same patterns.
  std::string old = withBits;
  for (const char* key : {",\"as\":1", ",\"bs\":1"}) {
    size_t at;
    while ((at = old.find(key)) != std::string::npos) old.erase(at, std::string(key).size());
  }
  assert(old.find("\"as\"") == std::string::npos);
  for (int loader = 0; loader < 2; ++loader) {
    scenes.wipeToZero();
    scenes.currentScene().songs[0].setStepsOverride(0, 9, true);  // must be replaced
    assert(loader == 0 ? scenes.loadScene(old) : scenes.loadSceneJson(old));
    assert(!anyBit(scenes.currentScene()));
    assert(scenes.currentScene().songs[0].length == 6);
  }
  std::puts("old file without keys loads with all bits 0; clean writer emits no keys: PASS");
  std::puts("test_song_cell_source_persistence PASS");
  return 0;
}
