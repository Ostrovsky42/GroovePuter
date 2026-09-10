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

uint8_t bitCount(uint16_t value) {
  uint8_t count = 0;
  while (value != 0) {
    count = static_cast<uint8_t>(count + (value & 1u));
    value = static_cast<uint16_t>(value >> 1u);
  }
  return count;
}

int run() {
  uint32_t profileCount = 0;
  uint32_t independentProfiles = 0;
  uint32_t familyNativeProfiles = 0;
  uint32_t autoRows = 0;
  uint32_t manualRows = 0;
  uint32_t nativeAutoRows = 0;
  uint32_t outsideIndependentAutoRows = 0;
  uint32_t dubFourFloorRows = 0;
  uint16_t dubFourFloorBassMask = 0;
  bool acidBaseIndependent = false;
  bool dubTechnoFamilyNative = false;
  bool dnbBaseFamilyNative = false;

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

      const bool familyNative =
          profile.bassSelectionPolicy == BassSelectionPolicy::FamilyNative;
      if (familyNative) ++familyNativeProfiles;
      else ++independentProfiles;

      if (mode == GenerativeMode::Acid && recipe == kBaseRecipeId) {
        acidBaseIndependent =
            profile.bassSelectionPolicy == BassSelectionPolicy::Independent;
      }
      if (mode == GenerativeMode::Reggae && recipe == 5) {
        dubTechnoFamilyNative = familyNative;
      }
      if (mode == GenerativeMode::DrumAndBass && recipe == kBaseRecipeId) {
        dnbBaseFamilyNative = familyNative;
      }

      for (uint16_t identity = 0; identity < 128; ++identity) {
        const GenerationCompositionResult composition =
            resolveGenerationComposition(settings, identityContext(identity));
        if (composition.status != GenerationCompositionStatus::Ok) {
          std::printf(
              "G4_I8_FAIL unresolved_auto mode=%u recipe=%u identity=%u status=%u archetype=%u\n",
              static_cast<unsigned>(modeOrdinal),
              static_cast<unsigned>(recipe),
              static_cast<unsigned>(identity),
              static_cast<unsigned>(composition.status),
              static_cast<unsigned>(composition.rhythmArchetypeId));
          return 1;
        }
        const bool native = compatible(composition);
        if (familyNative && !native) {
          const ReferenceVocabulary::Definition* definition =
              ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
          std::printf(
              "G4_I8_FAIL family_native_escape mode=%u recipe=%u identity=%u archetype=%u family=%u bass=%u\n",
              static_cast<unsigned>(modeOrdinal),
              static_cast<unsigned>(recipe),
              static_cast<unsigned>(identity),
              static_cast<unsigned>(composition.rhythmArchetypeId),
              definition == nullptr ? 255u : static_cast<unsigned>(definition->family),
              static_cast<unsigned>(composition.bassRhythm));
          return 1;
        }
        if (native) ++nativeAutoRows;
        else ++outsideIndependentAutoRows;
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
          const ReferenceVocabulary::Definition* definition =
              ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
          const bool native = definition != nullptr && compatible(composition);
          if (composition.status != GenerationCompositionStatus::Ok ||
              definition == nullptr || (familyNative && !native)) {
            std::printf(
                "G4_I8_FAIL manual_policy mode=%u recipe=%u identity=%u policy=%u archetype=%u family=%u bass=%u status=%u\n",
                static_cast<unsigned>(modeOrdinal),
                static_cast<unsigned>(recipe),
                static_cast<unsigned>(identity),
                static_cast<unsigned>(profile.bassSelectionPolicy),
                static_cast<unsigned>(composition.rhythmArchetypeId),
                definition == nullptr ? 255u : static_cast<unsigned>(definition->family),
                static_cast<unsigned>(composition.bassRhythm),
                static_cast<unsigned>(composition.status));
            return 1;
          }

          if (mode == GenerativeMode::Reggae && recipe == 5 &&
              definition->family == RhythmFamily::FourFloor) {
            const uint8_t bass = static_cast<uint8_t>(composition.bassRhythm);
            if (bass < 16u) {
              dubFourFloorBassMask = static_cast<uint16_t>(
                  dubFourFloorBassMask | (uint16_t{1} << bass));
            }
            ++dubFourFloorRows;
          }
          ++manualRows;
        }
      }
    }
  }

  if (!acidBaseIndependent) {
    std::puts("G4_I8_FAIL acid_base_must_remain_independent");
    return 1;
  }
  if (!dubTechnoFamilyNative) {
    std::puts("G4_I8_FAIL dub_techno_must_be_family_native");
    return 1;
  }
  if (!dnbBaseFamilyNative) {
    std::puts("G4_I8_FAIL dnb_base_must_be_family_native");
    return 1;
  }

  const uint8_t dubFourFloorVariants = bitCount(dubFourFloorBassMask);
  if (dubFourFloorRows == 0 || dubFourFloorVariants < 2) {
    std::printf(
        "G4_I8_FAIL dub_four_floor_collapse rows=%u variants=%u mask=0x%04x\n",
        static_cast<unsigned>(dubFourFloorRows),
        static_cast<unsigned>(dubFourFloorVariants),
        static_cast<unsigned>(dubFourFloorBassMask));
    return 1;
  }

  std::printf(
      "G4_I8_SUMMARY profiles=%u independent_profiles=%u family_native_profiles=%u "
      "auto_rows=%u native_auto_rows=%u outside_independent_auto_rows=%u "
      "manual_rows=%u dub_four_floor_rows=%u dub_four_floor_variants=%u\n",
      static_cast<unsigned>(profileCount),
      static_cast<unsigned>(independentProfiles),
      static_cast<unsigned>(familyNativeProfiles),
      static_cast<unsigned>(autoRows),
      static_cast<unsigned>(nativeAutoRows),
      static_cast<unsigned>(outsideIndependentAutoRows),
      static_cast<unsigned>(manualRows),
      static_cast<unsigned>(dubFourFloorRows),
      static_cast<unsigned>(dubFourFloorVariants));
  std::puts("G4-I8 bass selection policy binding: PASS");
  return 0;
}

}  // namespace

int main() { return run(); }
