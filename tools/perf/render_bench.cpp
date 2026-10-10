// 0.9.19 render benchmark: the real MiniAcid engine renders a fixed busy scene
// (two synths playing every sixteenth with slides and accents, a full drum
// bar) and reports host time per 512-frame block. Absolute host times are not
// the Cardputer's; the split between configurations, and a gprof build of
// this tool, show where the render time goes.
// Usage: render_bench [drums=808] [synthB=TB303] [blocks=2000] [mute=none|a|b|drums]
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/audio_config.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/miniacid_engine.h"
#include "src/platform/cardputer_material_publication_session.h"

SerialMock Serial;
SDMock SD;

namespace {

void fillSynth(SynthPattern& p, int root) {
  static const int kLine[16] = {0, 12, 0, 3, 0, 7, 12, 10, 0, 12, 5, 3, 0, 7, 10, 12};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    p.steps[i].note = static_cast<int8_t>(root + kLine[i]);
    p.steps[i].slide = (i % 4) == 3;
    p.steps[i].accent = (i % 8) == 4;
  }
}

void fillDrums(DrumPatternSet& d) {
  auto hit = [&](int voice, int step, bool accent = false) {
    d.voices[voice].steps[step].hit = 1;
    d.voices[voice].steps[step].accent = accent ? 1 : 0;
    d.voices[voice].steps[step].velocity = 100;
    d.voices[voice].steps[step].probability = 100;
  };
  for (int s = 0; s < 16; s += 4) hit(0, s, s == 0);   // kick
  hit(1, 4); hit(1, 12);                              // snare
  for (int s = 0; s < 16; ++s) hit(2, s, s % 4 == 2);  // closed hat
  hit(3, 14);                                         // open hat
  hit(7, 4); hit(7, 12);                              // clap
}

double rms(const std::vector<int16_t>& pcm) {
  double sum = 0.0;
  for (int16_t s : pcm) sum += static_cast<double>(s) * s;
  return pcm.empty() ? 0.0 : std::sqrt(sum / pcm.size());
}

}  // namespace

int main(int argc, char** argv) {
  const std::string drums = argc > 1 ? argv[1] : "808";
  const std::string synthB = argc > 2 ? argv[2] : "TB303";
  const int blocks = argc > 3 ? std::atoi(argv[3]) : 2000;
  const std::string mute = argc > 4 ? argv[4] : "none";

  const auto root = std::filesystem::temp_directory_path() / "gp_render_bench";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("render_bench", 0);
  PatternPagingService::setProjectName("render_bench");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(static_cast<float>(kSampleRate), &storage);
  engine.init();
  engine.setSongMode(false);
  engine.setBpm(130.0f);
  engine.setDrumEngine(drums);
  engine.setSynthEngine(0, "TB303");
  engine.setSynthEngine(1, synthB);

  Scene& scene = engine.sceneManager().currentScene();
  for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
    fillSynth(scene.synthABanks[0].patterns[i], 36);
    fillSynth(scene.synthBBanks[0].patterns[i], 48);
    fillDrums(scene.drumBanks[0].patterns[i]);
  }
  if (mute == "a") engine.toggleMute303(0);
  if (mute == "b") engine.toggleMute303(1);
  if (mute == "drums") {
    engine.toggleMuteKick(); engine.toggleMuteSnare(); engine.toggleMuteHat();
    engine.toggleMuteOpenHat(); engine.toggleMuteMidTom(); engine.toggleMuteHighTom();
    engine.toggleMuteRim(); engine.toggleMuteClap();
  }

  std::vector<int16_t> block(kBlockFrames);
  std::vector<int16_t> stopped;
  for (int b = 0; b < 40; ++b) {
    engine.generateAudioBuffer(block.data(), kBlockFrames);
    stopped.insert(stopped.end(), block.begin(), block.end());
  }

  engine.start();
  std::vector<int16_t> played;
  played.reserve(static_cast<size_t>(blocks) * kBlockFrames);
  std::vector<double> us(blocks);
  for (int b = 0; b < blocks; ++b) {
    const auto t0 = std::chrono::steady_clock::now();
    engine.generateAudioBuffer(block.data(), kBlockFrames);
    const auto t1 = std::chrono::steady_clock::now();
    us[b] = std::chrono::duration<double, std::micro>(t1 - t0).count();
    played.insert(played.end(), block.begin(), block.end());
  }
  engine.stop();

  double sum = 0.0, mx = 0.0;
  for (double v : us) { sum += v; if (v > mx) mx = v; }
  std::printf("drums=%s synthA=%s synthB=%s mute=%s blocks=%d avg=%.1fus max=%.1fus "
              "rmsStop=%.0f rmsPlay=%.0f\n",
              engine.currentDrumEngineName().c_str(),
              engine.currentSynthEngineName(0).c_str(),
              engine.currentSynthEngineName(1).c_str(), mute.c_str(), blocks,
              sum / blocks, mx, rms(stopped), rms(played));
  return 0;
}
