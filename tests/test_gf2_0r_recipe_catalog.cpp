#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"

using namespace GroovePuterRhythm;

namespace {

struct ReleasedRecipes {
  GenerativeMode genre;
  std::array<GenreRecipeId, 6> recipes;
  uint8_t count;
};

constexpr ReleasedRecipes kFinal099Catalog[] = {
    {GenerativeMode::Acid, {0, 6, 7, 0, 0, 0}, 3},
    {GenerativeMode::Outrun, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::Darksynth, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::Electro, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::Rave, {0, 4, 0, 0, 0, 0}, 2},
    {GenerativeMode::Reggae, {0, 5, 10, 11, 0, 0}, 4},
    {GenerativeMode::TripHop, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::Broken, {0, 1, 2, 3, 8, 9}, 6},
    {GenerativeMode::Chip, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::House, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::Techno, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::HipHop, {0, 16, 17, 0, 0, 0}, 3},
    {GenerativeMode::FunkSoul, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::UkGarage, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::DrumAndBass, {0, 0, 0, 0, 0, 0}, 1},
    {GenerativeMode::LoFi, {0, 12, 13, 14, 15, 0}, 5},
};

GenreSettings settingsFor(GenerativeMode genre, GenreRecipeId recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(genre);
  settings.recipe = recipe;
  return settings;
}

}  // namespace

int main() {
  static_assert(std::size(kFinal099Catalog) == kGenerativeModeCount);

  uint8_t profileCount = 0;
  uint8_t nonBaseCount = 0;
  for (const ReleasedRecipes& expected : kFinal099Catalog) {
    assert(availableRecipeCount(expected.genre) == expected.count);
    bool seen[256]{};
    for (uint8_t ordinal = 0; ordinal < expected.count; ++ordinal) {
      GenreRecipeId actual = 255;
      assert(availableRecipeAt(expected.genre, ordinal, actual));
      assert(actual == expected.recipes[ordinal]);
      assert(!seen[actual]);
      seen[actual] = true;
      assert(isRecipeAvailable(expected.genre, actual));

      const GenerationProfileView profile =
          generationProfileFor(settingsFor(expected.genre, actual));
      assert(profile.generativeMode == static_cast<uint8_t>(expected.genre));
      assert(profile.recipe == actual);
      ++profileCount;
      if (actual != kBaseRecipeId) ++nonBaseCount;
    }

    GenreRecipeId unchanged = 254;
    assert(!availableRecipeAt(expected.genre, expected.count, unchanged));
    assert(unchanged == 254);
    assert(isRecipeAvailable(expected.genre, kBaseRecipeId));
  }

  assert(profileCount == 33);
  assert(nonBaseCount == 17);

  assert(!isRecipeAvailable(GenerativeMode::Techno, 6));
  assert(!isRecipeAvailable(GenerativeMode::Acid, kDustyJazzRecipeId));
  const GenerationProfileView fallback =
      generationProfileFor(settingsFor(GenerativeMode::Techno, 6));
  assert(fallback.generativeMode == static_cast<uint8_t>(GenerativeMode::Techno));
  assert(fallback.recipe == kBaseRecipeId);

  const auto invalidGenre = static_cast<GenerativeMode>(kGenerativeModeCount);
  GenreRecipeId invalidOutput = 253;
  assert(availableRecipeCount(invalidGenre) == 0);
  assert(!availableRecipeAt(invalidGenre, 0, invalidOutput));
  assert(invalidOutput == 253);
  assert(!isRecipeAvailable(invalidGenre, kBaseRecipeId));

  std::puts("GF2-0R final v0.9.9 Recipe catalog: PASS profiles=33 non_base=17");
}
