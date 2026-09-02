#include <cassert>
#include <cstdint>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"

using namespace GroovePuterRhythm;

namespace {

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

GenerationContext contextFor(uint8_t mode, uint8_t recipe, uint16_t ordinal) {
  GenerationContext generation{};
  generation.projectSeed =
      0x15A00000u | (static_cast<uint32_t>(mode) << 8u) | recipe;
  generation.phraseOrdinal = ordinal;
  return generation;
}

bool isExactProfile(GenerativeMode mode, uint8_t recipe) {
  const GenerationProfileView profile =
      generationProfileFor(settingsFor(mode, recipe));
  return profile.progressions.candidates != nullptr && profile.recipe == recipe;
}

}  // namespace

int main() {
  bool selected[static_cast<uint8_t>(ProgressionId::Count)]{};
  uint16_t exactProfiles = 0;

  for (uint8_t modeValue = 0; modeValue < kGenerativeModeCount; ++modeValue) {
    const GenerativeMode mode = static_cast<GenerativeMode>(modeValue);
    for (uint8_t recipe = 0; recipe <= kDustyJazzRecipeId; ++recipe) {
      if (!isExactProfile(mode, recipe)) continue;
      ++exactProfiles;
      const GenreSettings settings = settingsFor(mode, recipe);
      for (uint16_t ordinal = 0; ordinal < 1024; ++ordinal) {
        const GenerationCompositionResult result =
            resolveGenerationComposition(
                settings, contextFor(modeValue, recipe, ordinal));
        assert(result.status == GenerationCompositionStatus::Ok);
        const uint8_t id = static_cast<uint8_t>(result.progression);
        assert(id > static_cast<uint8_t>(ProgressionId::Auto));
        assert(id < static_cast<uint8_t>(ProgressionId::Count));
        selected[id] = true;
      }
    }
  }

  assert(exactProfiles == 33);
  for (uint8_t id = static_cast<uint8_t>(ProgressionId::StaticModal);
       id < static_cast<uint8_t>(ProgressionId::Count); ++id) {
    assert(selected[id]);
  }
  return 0;
}
