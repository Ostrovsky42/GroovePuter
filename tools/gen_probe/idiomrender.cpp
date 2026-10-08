// Listening render (analysis): per genre, a full G, then a 4-bar G phrase into
// both synths' Melodies; 8 bars to WAV. Renders the tree it is built in.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/audio_config.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_melody.h"
#include "src/generation/migration/quantized_generation_commit.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/phrase_source_toggle.h"

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

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";
  const std::string tag = argc > 2 ? argv[2] : "x";
  struct G { GenerativeMode mode; const char* name; float bpm; };
  const G genres[] = {{GenerativeMode::Acid, "acid", 128.0f},
                      {GenerativeMode::House, "house", 122.0f},
                      {GenerativeMode::HipHop, "hiphop", 90.0f},
                      {GenerativeMode::UkGarage, "ukg", 132.0f},
                      {GenerativeMode::LoFi, "lofi", 78.0f},
                      {GenerativeMode::Outrun, "outrun", 108.0f},
                      {GenerativeMode::Darksynth, "darksynth", 120.0f},
                      {GenerativeMode::Techno, "techno", 126.0f},
                      {GenerativeMode::FunkSoul, "funksoul", 104.0f},
                      {GenerativeMode::Electro, "electro", 124.0f},
                      {GenerativeMode::Broken, "broken", 120.0f},
                      {GenerativeMode::DrumAndBass, "dnb", 172.0f},
                      {GenerativeMode::Chip, "chip", 140.0f}};
  const std::string only = argc > 4 ? argv[4] : "";
  for (const auto& g : genres) {
    if (!only.empty() && only.find(std::string(",") + g.name + ",") == std::string::npos) continue;
    const uint32_t takes = argc > 3 ? static_cast<uint32_t>(std::atoi(argv[3])) : 2;
    for (uint32_t take = 1; take <= takes; ++take) {
      const auto root = std::filesystem::temp_directory_path() / ("gp_idiomrender_" + tag);
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
      std::filesystem::create_directories(root);
      std::filesystem::current_path(root);
      SD.setRoot(root);
      GroovePuterPlatform::clearMaterialPublication("idiomrender", 0);
      PatternPagingService::setProjectName("idiomrender");
      SceneStorageSdl storage;
      storage.setCurrentSceneName("default");
      MiniAcid engine(static_cast<float>(kSampleRate), &storage);
      engine.init();
      engine.setSongMode(false);
      engine.genreManager().setGenerativeMode(g.mode);
      engine.genreManager().setRecipe(0);
      engine.setBpm(g.bpm);
      auto& scene = engine.sceneManager().currentScene();
      for (uint32_t press = 0; press < take; ++press) {
        R::regenerateWithQuantizedCommit(engine, scene.genre, engine.grooveboxMode(), false, g.bpm);
      }
      for (int voice = 0; voice < 2; ++voice) {
        PhraseRuntime::RuntimeSynthEventBuffer phrase{};
        if (GeneratedMelody::generate(engine, voice, 4, take, phrase) != GeneratedMelody::Status::Ready) {
          std::printf("%s %s take%u voice%d: generate failed\n", tag.c_str(), g.name, take, voice);
          continue;
        }
        PhraseSourceToggle::AudioGuard none;
        if (!PhraseSourceToggle::makePhrase(engine, none, voice)) {
          std::printf("%s %s voice%d: makePhrase failed\n", tag.c_str(), g.name, voice);
          continue;
        }
        (void)RuntimePhraseEdit::commit(engine.currentPhraseBuffer(voice), phrase);
        std::printf("%s %s take%u %s: %u notes\n", tag.c_str(), g.name, take, voice ? "B" : "A", phrase.count);
      }
      engine.start();
      const double seconds = 8 * 4 * 60.0 / g.bpm;
      std::vector<int16_t> pcm(static_cast<size_t>(seconds * kSampleRate));
      for (size_t at = 0; at < pcm.size(); at += 512) {
        const size_t n = std::min<size_t>(512, pcm.size() - at);
        engine.generateAudioBuffer(pcm.data() + at, n);
      }
      writeWav(out + "/" + tag + "_" + g.name + "_take" + std::to_string(take) + ".wav", pcm, kSampleRate);
    }
  }
  return 0;
}
