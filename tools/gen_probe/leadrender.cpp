// Listening render (analysis): genre -> G on Synth A and B -> 8 bars to WAV.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/audio_config.h"
#include "src/audio/pattern_paging.h"
#include "src/generation/migration/quantized_generation_commit.h"
#include "src/platform/cardputer_material_publication_session.h"

SerialMock Serial;
SDMock SD;
namespace R = GroovePuterRhythm;

static void writeWav(const std::string& path, const std::vector<int16_t>& pcm, uint32_t rate) {
  FILE* f = std::fopen(path.c_str(), "wb");
  const uint32_t data = static_cast<uint32_t>(pcm.size() * 2);
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  std::fwrite("data", 1, 4, f); u32(data);
  std::fwrite(pcm.data(), 2, pcm.size(), f);
  std::fclose(f);
}

static std::string nm(int n) {
  static const char* k[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (n < 0) return ".";
  return std::string(k[n % 12]) + std::to_string(n / 12 - 1);
}

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";
  const std::string tag = argc > 2 ? argv[2] : "x";
  struct G { GenerativeMode mode; const char* name; float bpm; };
  const G genres[] = {{GenerativeMode::Acid, "acid", 132.0f},
                      {GenerativeMode::Outrun, "outrun", 108.0f},
                      {GenerativeMode::House, "house", 122.0f}};
  for (const auto& g : genres) {
    for (int take = 0; take < 2; ++take) {
      const auto root = std::filesystem::temp_directory_path() / ("gp_leadrender_" + tag);
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
      std::filesystem::create_directories(root);
      std::filesystem::current_path(root);
      SD.setRoot(root);
      GroovePuterPlatform::clearMaterialPublication("leadrender", 0);
      PatternPagingService::setProjectName("leadrender");
      SceneStorageSdl storage;
      storage.setCurrentSceneName("default");
      MiniAcid engine(static_cast<float>(kSampleRate), &storage);
      engine.init();
      engine.setSongMode(false);
      engine.genreManager().setGenerativeMode(g.mode);
      engine.genreManager().setRecipe(0);
      engine.setBpm(g.bpm);
      for (int press = 0; press <= take; ++press) {
        R::regenerateSynthWithQuantizedCommit(engine, 0);
        R::regenerateSynthWithQuantizedCommit(engine, 1);
      }
      auto& scene = engine.sceneManager().currentScene();
      for (int v = 0; v < 2; ++v) {
        const auto& p = v == 0 ? scene.synthABanks[engine.current303BankIndex(0)].patterns[engine.current303PatternIndex(0)]
                               : scene.synthBBanks[engine.current303BankIndex(1)].patterns[engine.current303PatternIndex(1)];
        std::printf("%s %s take%d %s:", tag.c_str(), g.name, take + 1, v ? "B" : "A");
        for (int i = 0; i < 16; ++i)
          std::printf(" %s%s%s", nm(p.steps[i].note).c_str(), p.steps[i].slide ? "~" : "",
                      p.steps[i].accent ? "!" : "");
        std::printf("\n");
      }
      engine.start();
      const double seconds = 8 * 4 * 60.0 / g.bpm;  // 8 bars
      std::vector<int16_t> pcm(static_cast<size_t>(seconds * kSampleRate));
      for (size_t at = 0; at < pcm.size(); at += 512) {
        const size_t n = std::min<size_t>(512, pcm.size() - at);
        engine.generateAudioBuffer(pcm.data() + at, n);
      }
      writeWav(out + "/" + tag + "_" + g.name + "_take" + std::to_string(take + 1) + ".wav", pcm, kSampleRate);
    }
  }
  return 0;
}
