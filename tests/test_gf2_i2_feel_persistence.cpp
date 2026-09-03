// GF2-I2 — FeelProfileId persistence is append-only.
//
// Scenes store the numeric FeelProfileId. Adding the AUTO selection mode must
// not renumber the four concrete profiles, must not reinterpret documents
// written before AUTO existed, and AUTO itself must survive a round trip.

#include "../platform_sdl/arduino_compat.h"
#include "../scenes.h"

#include <cstdio>
#include <string>

SerialMock Serial;
SDMock SD;

namespace {

int g_failures = 0;

void expectTrue(const char* label, bool condition) {
  if (condition) {
    std::printf("%-58s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-58s FAIL\n", label);
  ++g_failures;
}

bool roundTripProfile(SceneManager& manager, uint8_t profile) {
  manager.loadDefaultScene();
  manager.currentScene().feel.timingProfile = profile;
  const std::string json = manager.dumpCurrentScene();
  manager.currentScene().feel = FeelSettings();
  if (!manager.loadScene(json)) return false;
  return manager.currentScene().feel.timingProfile == profile;
}

}  // namespace

int main() {
  using GroovePuterRhythm::FeelProfileId;

  expectTrue("Straight is 0", static_cast<uint8_t>(FeelProfileId::Straight) == 0);
  expectTrue("SwingCompatible is 1",
             static_cast<uint8_t>(FeelProfileId::SwingCompatible) == 1);
  expectTrue("LaidBack is 2", static_cast<uint8_t>(FeelProfileId::LaidBack) == 2);
  expectTrue("PushPullControlled is 3",
             static_cast<uint8_t>(FeelProfileId::PushPullControlled) == 3);
  expectTrue("Auto is appended as 4",
             static_cast<uint8_t>(FeelProfileId::Auto) == 4);

  SceneManager manager;

  for (uint8_t profile = 0;
       profile < static_cast<uint8_t>(FeelProfileId::Count); ++profile) {
    expectTrue("every selectable FEEL profile round-trips",
               roundTripProfile(manager, profile));
  }

  // A document written before AUTO existed must still decode as the concrete
  // profile it recorded, never as AUTO.
  manager.loadDefaultScene();
  manager.currentScene().feel.timingProfile =
      static_cast<uint8_t>(FeelProfileId::LaidBack);
  const std::string legacy = manager.dumpCurrentScene();
  expectTrue("legacy documents still store the numeric profile",
             legacy.find("\"profile\":2") != std::string::npos);
  manager.currentScene().feel = FeelSettings();
  expectTrue("legacy document loads", manager.loadScene(legacy));
  expectTrue("legacy LAID BACK stays LAID BACK",
             manager.currentScene().feel.timingProfile ==
                 static_cast<uint8_t>(FeelProfileId::LaidBack));

  // A fresh Scene keeps the established default; AUTO is opt-in.
  manager.loadDefaultScene();
  expectTrue("fresh scenes default to STRAIGHT, not AUTO",
             manager.currentScene().feel.timingProfile ==
                 static_cast<uint8_t>(FeelProfileId::Straight));

  // An out-of-range profile still falls back to STRAIGHT.
  manager.loadDefaultScene();
  manager.currentScene().feel.timingProfile =
      static_cast<uint8_t>(FeelProfileId::Auto);
  std::string corrupt = manager.dumpCurrentScene();
  const size_t pos = corrupt.find("\"profile\":4");
  expectTrue("AUTO is written as 4", pos != std::string::npos);
  if (pos != std::string::npos) {
    corrupt.replace(pos, std::string("\"profile\":4").size(), "\"profile\":9");
    expectTrue("corrupt document loads", manager.loadScene(corrupt));
    expectTrue("out-of-range profile falls back to STRAIGHT",
               manager.currentScene().feel.timingProfile ==
                   static_cast<uint8_t>(FeelProfileId::Straight));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I2 feel persistence: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("GF2-I2 feel persistence: PASS\n");
  return 0;
}
