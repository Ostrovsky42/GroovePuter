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
  const int modes[] = {0, 9, 11, 13, 15};
  if (argc > 2 && std::string(argv[2]) == "distinct") {
    std::printf("genre\tvoice\tdistinct/8\tbar1 distinct/8\n");
    for (int mode : modes) {
      engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
      engine.genreManager().setRecipe(0);
      for (int voice = 0; voice < 2; ++voice) {
        std::set<std::string> whole, first;
        for (uint32_t salt = 1; salt <= 8; ++salt) {
          PhraseRuntime::RuntimeSynthEventBuffer a{};
          GeneratedMelody::generate(engine, voice, bars, salt, a);
          std::string w, f;
          for (uint16_t i = 0; i < a.count; ++i) {
            const std::string e = std::to_string(a.events[i].startTick) + ":" + std::to_string(a.events[i].note) + ",";
            w += e;
            if (a.events[i].startTick < PhraseRuntime::kTicksPerBar) f += e;
          }
          whole.insert(w); first.insert(f);
        }
        std::printf("%s\t%s\t%zu\t%zu\n", kGenre[mode], voice ? "B" : "A", whole.size(), first.size());
      }
    }
    return 0;
  }

  for (int mode : modes) {
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
    engine.genreManager().setRecipe(0);
    for (uint32_t salt = 1; salt <= 2; ++salt)
    for (int voice = 0; voice < 2; ++voice) {
      PhraseRuntime::RuntimeSynthEventBuffer a{};
      const auto st = GeneratedMelody::generate(engine, voice, bars, salt, a);
      for (int b = 0; b < bars; ++b) {
        std::printf("%-8s s%u %s bar%d %s:", kGenre[mode], salt, voice ? "B" : "A", b + 1, GeneratedMelody::statusText(st));
        for (uint16_t i = 0; i < a.count; ++i)
          if (a.events[i].startTick / PhraseRuntime::kTicksPerBar == b)
            std::printf(" %u:%s/%u", (a.events[i].startTick % PhraseRuntime::kTicksPerBar) / 24,
                        nm(a.events[i].note).c_str(), a.events[i].durationSubticks / 384);
        std::printf("\n");
      }
    }
  }
  return 0;
}
