#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/roles/bass_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

GenerationContext identityContext(uint16_t identity) {
  GenerationContext context{};
  context.projectSeed = 0x47344938u;
  context.phraseOrdinal = identity;
  return context;
}

GenreSettings settingsFor(GenerativeMode mode, GenreRecipeId recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = static_cast<uint8_t>(recipe);
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

bool compatible(const GenerationCompositionResult& composition) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
  if (definition == nullptr) return false;
  return isBassRhythmCompatibleWithFamily(definition->family,
                                           composition.bassRhythm);
}

int run() {
  uint32_t profileCount = 0;
  uint32_t autoRows = 0;
  uint32_t manualRows = 0;

  for (uint8_t modeOrdinal = 0;
       modeOrdinal < static_cast<uint8_t>(kGenerativeModeCount);
       ++modeOrdinal) {
    const auto mode = static_cast<GenerativeMode>(modeOrdinal);
    const uint8_t recipeCount = availableRecipeCount(mode);
    for (uint8_t recipeOrdinal = 0; recipeOrdinal < recipeCount;
         ++recipeOrdinal) {
      GenreRecipeId recipe = kBaseRecipeId;
      if (!availableRecipeAt(mode, recipeOrdinal, recipe)) {
        std::printf("G4_I8_FAIL catalog mode=%u recipe_ordinal=%u\n",
                    static_cast<unsigned>(modeOrdinal),
                    static_cast<unsigned>(recipeOrdinal));
        return 1;
      }

      GenreSettings settings = settingsFor(mode, recipe);
      const GenerationProfileView profile = generationProfileFor(settings);
      if (!isValidGenerationProfile(profile)) {
        std::printf("G4_I8_FAIL invalid_profile mode=%u recipe=%u\n",
                    static_cast<unsigned>(modeOrdinal),
                    static_cast<unsigned>(recipe));
        return 1;
      }
      ++profileCount;

      for (uint16_t identity = 0; identity < 128; ++identity) {
        const GenerationCompositionResult composition =
            resolveGenerationComposition(settings, identityContext(identity));
        if (composition.status != GenerationCompositionStatus::Ok) {
          std::printf(
              "G4_I8_FAIL unresolved_auto mode=%u recipe=%u identity=%u status=%u\n",
              static_cast<unsigned>(modeOrdinal),
              static_cast<unsigned>(recipe),
              static_cast<unsigned>(identity),
              static_cast<unsigned>(composition.status));
          return 1;
        }
        if (!compatible(composition)) {
          const ReferenceVocabulary::Definition* definition =
              ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
          std::printf(
              "G4_I8_FAIL incompatible_auto mode=%u recipe=%u identity=%u archetype=%u family=%u bass=%u\n",
              static_cast<unsigned>(modeOrdinal),
              static_cast<unsigned>(recipe),
              static_cast<unsigned>(identity),
              static_cast<unsigned>(composition.rhythmArchetypeId),
              definition == nullptr ? 255u : static_cast<unsigned>(definition->family),
              static_cast<unsigned>(composition.bassRhythm));
          return 1;
        }
        ++autoRows;
      }

      settings.rhythmSelectionMode =
          static_cast<uint8_t>(RhythmSelectionMode::Manual);
      for (uint8_t rhythmIndex = 0; rhythmIndex < profile.rhythms.count;
           ++rhythmIndex) {
        settings.rhythmArchetypeId =
            profile.rhythms.candidates[rhythmIndex].archetypeId;
        for (uint16_t identity = 0; identity < 32; ++identity) {
          const GenerationCompositionResult composition =
              resolveGenerationComposition(settings, identityContext(identity));
          if (composition.status != GenerationCompositionStatus::Ok ||
              !compatible(composition)) {
            const ReferenceVocabulary::Definition* definition =
                ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
            std::printf(
                "G4_I8_FAIL incompatible_manual mode=%u recipe=%u identity=%u archetype=%u family=%u bass=%u status=%u\n",
                static_cast<unsigned>(modeOrdinal),
                static_cast<unsigned>(recipe),
                static_cast<unsigned>(identity),
                static_cast<unsigned>(composition.rhythmArchetypeId),
                definition == nullptr ? 255u : static_cast<unsigned>(definition->family),
                static_cast<unsigned>(composition.bassRhythm),
                static_cast<unsigned>(composition.status));
            return 1;
          }
          ++manualRows;
        }
      }
    }
  }

  std::printf("G4_I8_SUMMARY profiles=%u auto_rows=%u manual_rows=%u\n",
              static_cast<unsigned>(profileCount),
              static_cast<unsigned>(autoRows),
              static_cast<unsigned>(manualRows));
  std::puts("G4-I8 bass family compatibility binding: PASS");
  return 0;
}

}  // namespace

int main() { return run(); }
