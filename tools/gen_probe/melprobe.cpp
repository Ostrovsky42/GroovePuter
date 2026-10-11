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
  std::printf("genre\tvoice\tstatus\tnotes/bar\tpitches\trange\tbarsDiffer\toverlap\tsameSaltSame\tnextSaltDiffers\n");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
    engine.genreManager().setRecipe(0);
    for (int voice = 0; voice < 2; ++voice) {
      PhraseRuntime::RuntimeSynthEventBuffer a{}, again{}, next{};
      const auto status = GeneratedMelody::generate(engine, voice, bars, 1, a);
      GeneratedMelody::generate(engine, voice, bars, 1, again);
      GeneratedMelody::generate(engine, voice, bars, 2, next);
      std::set<int> pitches;
      int lo = 999, hi = -1;
      std::vector<std::string> barSig(bars);
      for (uint16_t i = 0; i < a.count; ++i) {
        pitches.insert(a.events[i].note);
        lo = std::min<int>(lo, a.events[i].note);
        hi = std::max<int>(hi, a.events[i].note);
        const int b = a.events[i].startTick / PhraseRuntime::kTicksPerBar;
        barSig[b] += std::to_string(a.events[i].startTick % PhraseRuntime::kTicksPerBar) + ":" +
                     std::to_string(a.events[i].note) + ",";
      }
      int differ = 0;
      for (int b = 1; b < bars; ++b) differ += barSig[b] != barSig[0];
      std::printf("%s\t%s\t%s\t%.1f\t%zu\t%d\t%d/%d\t%s\t%s\t%s\n", kGenre[mode], voice ? "B" : "A",
                  GeneratedMelody::statusText(status), double(a.count) / bars, pitches.size(),
                  a.count ? hi - lo : 0, differ, bars - 1,
                  RuntimePhraseEdit::hasOverlappingNotes(a) ? "YES" : "no",
                  RuntimePhraseEdit::same(a, again) ? "yes" : "NO",
                  RuntimePhraseEdit::same(a, next) ? "NO" : "yes");
      if (print && voice == 1 && mode < 3) {
        for (int b = 0; b < bars; ++b) {
          std::printf("#  %s B bar%d:", kGenre[mode], b + 1);
          for (uint16_t i = 0; i < a.count; ++i)
            if (a.events[i].startTick / PhraseRuntime::kTicksPerBar == b)
              std::printf(" %u:%s", (a.events[i].startTick % PhraseRuntime::kTicksPerBar) / 24,
                          nm(a.events[i].note).c_str());
          std::printf("\n");
        }
      }
    }
  }
  return 0;
}
