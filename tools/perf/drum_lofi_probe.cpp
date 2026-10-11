#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "src/audio/audio_config.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/miniacid_engine.h"

SerialMock Serial;
SDMock SD;

static uint64_t render(bool lofi) {
  const auto root = std::filesystem::temp_directory_path() /
                    (lofi ? "gp_lofi_on" : "gp_lofi_off");
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  PatternPagingService::setProjectName(lofi ? "gp_lofi_on" : "gp_lofi_off");
  MiniAcid engine(static_cast<float>(kSampleRate), nullptr);
  engine.sceneManager().currentScene().feel.lofiEnabled = lofi;
  engine.sceneManager().currentScene().feel.lofiAmount = 80;
  engine.init();
  engine.toggleMute303(0);
  engine.toggleMute303(1);
  Scene& scene = engine.sceneManager().currentScene();
  auto& drums = scene.drumBanks[0].patterns[0];
  for (int step = 0; step < 16; step += 4) {
    drums.voices[0].steps[step].hit = 1;
    drums.voices[0].steps[step].velocity = 110;
    drums.voices[0].steps[step].probability = 100;
    drums.voices[2].steps[step].hit = 1;
    drums.voices[2].steps[step].velocity = 100;
    drums.voices[2].steps[step].probability = 100;
  }
  engine.start();
  std::vector<int16_t> block(kBlockFrames);
  uint64_t hash = 1469598103934665603ULL;
  for (int b = 0; b < 80; ++b) {
    engine.generateAudioBuffer(block.data(), block.size());
    for (int16_t sample : block) {
      hash ^= static_cast<uint16_t>(sample);
      hash *= 1099511628211ULL;
    }
  }
  return hash;
}

int main() {
  const auto dry = render(false);
  const auto wet = render(true);
  std::printf("drum LoFi dry=%016llx wet=%016llx\n",
              static_cast<unsigned long long>(dry),
              static_cast<unsigned long long>(wet));
  assert(dry != wet && "FEEL LoFi must change the drum bus audio");
}
