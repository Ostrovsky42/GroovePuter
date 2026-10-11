// Melody phrase probe (analysis): GeneratedMelody::generate per genre, 4 bars.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_melody.h"
#include "src/state/generation_shape_state.h"
#include "src/platform/cardputer_material_publication_session.h"

SerialMock Serial;
SDMock SD;

static const char* kGenre[] = {"Acid", "Outrun", "Darksynth", "Electro", "Rave",
                               "Reggae", "TripHop", "Broken", "Chip", "House",
                               "Techno", "HipHop", "FunkSoul", "UkGarage", "DnB", "LoFi"};

static std::string nm(int n) {
  static const char* k[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return std::string(k[n % 12]) + std::to_string(n / 12 - 1);
}

int main(int argc, char** argv) {
  const uint8_t bars = argc > 1 ? static_cast<uint8_t>(std::atoi(argv[1])) : 4;
  const bool print = argc > 2;
  const auto root = std::filesystem::temp_directory_path() / "gp_melprobe";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("melprobe", 0);
  PatternPagingService::setProjectName("melprobe");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(44100, &storage);
  engine.init();
  engine.setSongMode(false);
  std::printf("genre\tCALM\tNORMAL\tLIVELY  (voice B notes, 6 presses x 4 bars)\n");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
    engine.genreManager().setRecipe(0);
    std::printf("%s", kGenre[mode]);
    for (int l = 0; l < 3; ++l) {
      GroovePuterState::setGenerationLiveliness(static_cast<GroovePuterState::GenerationLiveliness>(l));
      unsigned total = 0;
      for (uint32_t salt = 1; salt <= 12; ++salt) {
        PhraseRuntime::RuntimeSynthEventBuffer a{};
        GeneratedMelody::generate(engine, 1, bars, salt, a);
        total += a.count;
      }
      std::printf("\t%u", total);
    }
    std::printf("\n");
  }
  return 0;
}
