#include "arduino_compat.h"

#include "../src/audio/audio_config.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/dsp/miniacid_engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

SerialMock Serial;
SDMock SD;

namespace {

struct FamilyCase {
  const char* label;
  GenerativeMode mode;
  GenreRecipeId recipe;
};

constexpr FamilyCase kFamilies[] = {
    {"HypnoticSparse", GenerativeMode::Techno, 11},
    {"LoFi", GenerativeMode::LoFi, kClassicChillRecipeId},
    {"Rave", GenerativeMode::Rave, kBaseRecipeId},
    {"DenseDnB", GenerativeMode::DrumAndBass, 2},
};

constexpr std::array<uint8_t, 4> kLengths = {1, 2, 4, 8};
constexpr int kWarmupIterations = 32;
constexpr int kMeasuredIterations = 256;
constexpr uint32_t kBenchmarkSeed = 0xE0090900u;
volatile uint64_t g_checksum = 0;

void configureFamily(MiniAcid& engine, const FamilyCase& family) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(family.mode);
  scene.genre.recipe = family.recipe;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.feel.patternBars = 1;

  for (int bank = 0; bank < kBankCount; ++bank) {
    for (int slot = 0; slot < Bank<SynthPattern>::kPatterns; ++slot) {
      scene.synthABanks[bank].patterns[slot] = SynthPattern{};
      scene.synthBBanks[bank].patterns[slot] = SynthPattern{};
      scene.drumBanks[bank].patterns[slot] = DrumPatternSet{};
    }
  }
}

uint64_t percentile(std::vector<uint64_t> values, double fraction) {
  std::sort(values.begin(), values.end());
  const std::size_t index = static_cast<std::size_t>(
      fraction * static_cast<double>(values.size() - 1));
  return values[index];
}

bool runPrepare(MiniAcid& engine,
                uint8_t bars,
                GeneratedPhraseSong::PreparedPhraseArrangement& prepared) {
  const bool ok = GeneratedPhraseSong::prepare(engine, bars, 0, prepared);
  if (ok) {
    g_checksum += static_cast<uint64_t>(prepared.result.firstGlobalPattern + 1);
    g_checksum += static_cast<uint64_t>(
        prepared.material[bars - 1].synthA.steps[0].velocity);
  }
  return ok;
}

void benchmarkCase(MiniAcid& engine,
                   const FamilyCase& family,
                   uint8_t bars) {
  configureFamily(engine, family);
  GeneratedPhraseSong::PreparedPhraseArrangement prepared{};

  for (int i = 0; i < kWarmupIterations; ++i) {
    if (!runPrepare(engine, bars, prepared)) {
      std::fprintf(stderr,
                   "E0A_BENCH ERROR family=%s bars=%u warmup_error=%d\n",
                   family.label,
                   static_cast<unsigned>(bars),
                   static_cast<int>(prepared.result.error));
      std::exit(2);
    }
  }

  std::vector<uint64_t> samples;
  samples.reserve(kMeasuredIterations);
  for (int i = 0; i < kMeasuredIterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    const bool ok = runPrepare(engine, bars, prepared);
    const auto end = std::chrono::steady_clock::now();
    if (!ok) {
      std::fprintf(stderr,
                   "E0A_BENCH ERROR family=%s bars=%u iteration=%d error=%d\n",
                   family.label,
                   static_cast<unsigned>(bars),
                   i,
                   static_cast<int>(prepared.result.error));
      std::exit(3);
    }
    samples.push_back(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count()));
  }

  const uint64_t medianNs = percentile(samples, 0.50);
  const uint64_t p95Ns = percentile(samples, 0.95);
  const uint64_t maxNs = *std::max_element(samples.begin(), samples.end());
  const bool atlasPath = AtlasRuntime::hasRecipe(family.recipe) &&
                         AtlasRuntime::variationCount(family.recipe) >= 3;

  std::printf(
      "E0A_BENCH family=%s mode=%s recipe=%u recipe_name=%s path=%s bars=%u "
      "iterations=%d median_total_us=%.3f median_per_bar_us=%.3f "
      "p95_total_us=%.3f max_total_us=%.3f\n",
      family.label,
      GenreCatalog::generativeModeName(family.mode),
      static_cast<unsigned>(family.recipe),
      GenreCatalog::recipeName(family.recipe),
      atlasPath ? "atlas" : "procedural",
      static_cast<unsigned>(bars),
      kMeasuredIterations,
      static_cast<double>(medianNs) / 1000.0,
      static_cast<double>(medianNs) /
          (1000.0 * static_cast<double>(bars)),
      static_cast<double>(p95Ns) / 1000.0,
      static_cast<double>(maxNs) / 1000.0);
}

}  // namespace

int main() {
  MiniAcid engine(kSampleRate, nullptr);
  engine.modeManager().setGenerationSeed(kBenchmarkSeed);

  std::printf(
      "E0A_BENCH host_clock=steady_clock warmup=%d iterations=%d seed=0x%08X\n",
      kWarmupIterations,
      kMeasuredIterations,
      static_cast<unsigned>(kBenchmarkSeed));
  for (const FamilyCase& family : kFamilies) {
    for (uint8_t bars : kLengths) {
      benchmarkCase(engine, family, bars);
    }
  }
  std::printf("E0A_BENCH checksum=%llu\n",
              static_cast<unsigned long long>(g_checksum));
  return 0;
}
