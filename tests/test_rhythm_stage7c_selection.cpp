#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

GenreSettings baseSettings(GenerativeMode mode) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = kBaseRecipeId;
  settings.morphTarget = kBaseRecipeId;
  settings.morphAmount = 0;
  return settings;
}

GenreSettings recipeSettings(GenerativeMode mode, uint8_t recipe) {
  GenreSettings settings = baseSettings(mode);
  settings.recipe = recipe;
  return settings;
}

struct ProfileSet {
  GenreSettings values[20]{};
  uint8_t count = 0;
};

ProfileSet productionProfiles() {
  ProfileSet profiles{};
  const GenerativeMode baseModes[] = {
      GenerativeMode::Acid,
      GenerativeMode::Outrun,
      GenerativeMode::Darksynth,
      GenerativeMode::Electro,
      GenerativeMode::Rave,
      GenerativeMode::Reggae,
      GenerativeMode::TripHop,
      GenerativeMode::Broken,
      GenerativeMode::Chip,
  };
  for (GenerativeMode mode : baseModes) {
    profiles.values[profiles.count++] = baseSettings(mode);
  }

  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Broken, 1);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Broken, 2);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Broken, 3);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Rave, 4);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Reggae, 5);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Acid, 6);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Acid, 7);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Broken, 8);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Broken, 9);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Reggae, 10);
  profiles.values[profiles.count++] = recipeSettings(GenerativeMode::Reggae, 11);
  return profiles;
}

bool contains(const GenreSettings& settings, RhythmArchetypeId archetypeId) {
  return isRhythmCompatible(settings, archetypeId);
}

GenreSettings profileFor(RhythmArchetypeId archetypeId) {
  const ProfileSet profiles = productionProfiles();
  for (uint8_t index = 0; index < profiles.count; ++index) {
    if (contains(profiles.values[index], archetypeId)) {
      return profiles.values[index];
    }
  }
  require(false, "production archetype has no compatible Genre/Variant");
  return {};
}

uint32_t drumFingerprint(const DrumPatternSet& drums) {
  uint32_t hash = 2166136261u;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = drums.voices[voice].steps[step];
      hash = (hash ^ static_cast<uint32_t>(event.hit)) * 16777619u;
      hash = (hash ^ static_cast<uint32_t>(event.accent)) * 16777619u;
      hash = (hash ^ static_cast<uint32_t>(event.velocity)) * 16777619u;
    }
  }
  return hash;
}

void testEveryProductionIdIsReachable() {
  bool compatible[static_cast<uint8_t>(ReferenceVocabulary::Archetype::Count)]{};
  bool selected[static_cast<uint8_t>(ReferenceVocabulary::Archetype::Count)]{};
  const ProfileSet profiles = productionProfiles();

  for (uint8_t profileIndex = 0; profileIndex < profiles.count; ++profileIndex) {
    const GenreSettings& settings = profiles.values[profileIndex];
    const uint8_t count = compatibleRhythmCount(settings);
    require(count > 0, "production profile has no compatible rhythm");
    for (uint8_t candidateIndex = 0; candidateIndex < count; ++candidateIndex) {
      const RhythmArchetypeId archetypeId =
          compatibleRhythmId(settings, candidateIndex);
      require(archetypeId != kNoArchetypeId,
              "canonical compatibility enumeration returned no ID");
      const ReferenceVocabulary::Definition* definition =
          ReferenceVocabulary::definitionForId(archetypeId);
      require(definition != nullptr,
              "compatibility table referenced a non-production ID");
      compatible[static_cast<uint8_t>(definition->key)] = true;
    }

    for (uint16_t ordinal = 0; ordinal < 512; ++ordinal) {
      GenerationContext generation{};
      generation.projectSeed =
          0x5A17C000u + static_cast<uint32_t>(profileIndex) * 0x101u;
      generation.phraseOrdinal = ordinal;
      const RhythmSelectionResult result =
          resolveRhythmSelection(settings, generation);
      require(result.status == RhythmSelectionStatus::Ok,
              "AUTO failed for a production profile");
      require(result.mode == RhythmSelectionMode::Auto,
              "AUTO returned MANUAL provenance");
      require(isRhythmCompatible(settings, result.archetypeId),
              "AUTO selected an incompatible rhythm");
      const ReferenceVocabulary::Definition* definition =
          ReferenceVocabulary::definitionForId(result.archetypeId);
      require(definition != nullptr, "AUTO returned an unknown ID");
      selected[static_cast<uint8_t>(definition->key)] = true;
    }
  }

  for (uint8_t index = 0;
       index < ReferenceVocabulary::definitionCount(); ++index) {
    require(compatible[index],
            "production rhythm is absent from all compatibility profiles");
    require(selected[index],
            "production rhythm was not selected by deterministic AUTO corpus");
  }
}

void testAutoIsDeterministicAndTableOrderIndependent() {
  const ProfileSet profiles = productionProfiles();
  for (uint8_t profileIndex = 0; profileIndex < profiles.count; ++profileIndex) {
    const GenreSettings& settings = profiles.values[profileIndex];
    const RhythmCompatibilityView forward = rhythmCompatibilityFor(settings);
    require(forward.count <= 24, "compatibility profile exceeds fixed capacity");
    RhythmCompatibilityCandidate reversed[24]{};
    for (uint8_t index = 0; index < forward.count; ++index) {
      reversed[index] = forward.candidates[forward.count - 1u - index];
    }
    const RhythmCompatibilityView reverseView{reversed, forward.count};
    const RhythmSelectionIntent intent{};

    for (uint16_t ordinal = 0; ordinal < 128; ++ordinal) {
      GenerationContext generation{};
      generation.projectSeed = 0x7C000000u + profileIndex;
      generation.phraseOrdinal = ordinal;
      const RhythmSelectionResult first =
          resolveRhythmSelection(settings, generation);
      const RhythmSelectionResult second =
          resolveRhythmSelection(settings, generation);
      const RhythmSelectionResult reversedResult =
          resolveRhythmSelectionFromView(reverseView, intent, generation);
      require(first.status == RhythmSelectionStatus::Ok &&
                  second.status == first.status &&
                  second.archetypeId == first.archetypeId,
              "same AUTO context changed identity");
      require(reversedResult.status == first.status &&
                  reversedResult.archetypeId == first.archetypeId,
              "compatibility declaration order changed AUTO identity");
    }
  }
}

void testManualIdentityAndRealizationVariation() {
  for (uint8_t definitionIndex = 0;
       definitionIndex < ReferenceVocabulary::definitionCount();
       ++definitionIndex) {
    const ReferenceVocabulary::Definition& definition =
        ReferenceVocabulary::definition(definitionIndex);
    GenreSettings settings = profileFor(definition.archetypeId);
    settings.rhythmSelectionMode =
        static_cast<uint8_t>(RhythmSelectionMode::Manual);
    settings.rhythmArchetypeId = definition.archetypeId;

    uint32_t firstFingerprint = 0;
    bool varied = false;
    for (int address = 0; address < 32; ++address) {
      StrongRhythmMigrationContext context{};
      context.patternAddress = static_cast<int16_t>(address);
      context.level = RealizationLevel::P2Variation;
      DrumPatternSet drums{};
      const StrongRhythmMigrationResult result =
          migrateStrongRhythmDrums(settings, context, drums);
      require(result.status == StrongRhythmMigrationStatus::Applied,
              "MANUAL production rhythm failed materialization");
      require(result.selectionMode == RhythmSelectionMode::Manual,
              "MANUAL selection lost provenance");
      require(result.archetype == definition.key,
              "seed/address changed MANUAL rhythm identity");
      const uint32_t fingerprint = drumFingerprint(drums);
      if (address == 0) firstFingerprint = fingerprint;
      else if (fingerprint != firstFingerprint) varied = true;
    }
    require(varied, "MANUAL rhythm identity has no P2 realization variation");

    for (uint8_t level = 0;
         level < static_cast<uint8_t>(RealizationLevel::Count); ++level) {
      StrongRhythmMigrationContext context{};
      context.patternAddress = 9;
      context.level = static_cast<RealizationLevel>(level);
      DrumPatternSet drums{};
      const StrongRhythmMigrationResult result =
          migrateStrongRhythmDrums(settings, context, drums);
      require(result.status == StrongRhythmMigrationStatus::Applied,
              "P-level rejected a MANUAL production rhythm");
      require(result.archetype == definition.key,
              "P1/P2/P3 changed MANUAL rhythm identity");
    }
  }
}

void testIncompatibleManualFallsBackToAuto() {
  GenreSettings settings = baseSettings(GenerativeMode::Electro);
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Manual);
  settings.rhythmArchetypeId = 711;  // Minimal/Techno/Chip, not Electro.
  GenerationContext generation{};
  generation.projectSeed = 0xBAD711u;
  generation.phraseOrdinal = 3;
  const RhythmSelectionResult result =
      resolveRhythmSelection(settings, generation);
  require(result.status == RhythmSelectionStatus::Ok,
          "incompatible MANUAL did not find AUTO fallback");
  require(result.mode == RhythmSelectionMode::Auto,
          "incompatible MANUAL did not normalize to AUTO");
  require(result.normalizedToAuto,
          "incompatible MANUAL fallback was not observable");
  require(result.archetypeId != 711 &&
              isRhythmCompatible(settings, result.archetypeId),
          "incompatible MANUAL ID escaped compatibility filter");
}

void testMismatchedVariantCannotMaskGenre() {
  GenreSettings corrupted = recipeSettings(GenerativeMode::Darksynth, 6);
  GenreSettings technoBase = baseSettings(GenerativeMode::Darksynth);
  require(compatibleRhythmCount(corrupted) == compatibleRhythmCount(technoBase),
          "mismatched Variant changed the base Genre compatibility size");
  for (uint8_t index = 0; index < compatibleRhythmCount(technoBase); ++index) {
    require(compatibleRhythmId(corrupted, index) ==
                compatibleRhythmId(technoBase, index),
            "mismatched Variant masked the base Genre profile");
  }
  require(!isRhythmCompatible(corrupted, 405) &&
              !isRhythmCompatible(corrupted, 408),
          "Chicago Jack rhythm leaked through Techno Genre");
  require(selectStrongRhythmRoute(corrupted) ==
              StrongRhythmRoute::TechnoBase,
          "mismatched Variant still changed the production migration route");
  StrongRhythmMigrationContext context{};
  context.patternAddress = 4;
  context.level = RealizationLevel::P2Variation;
  DrumPatternSet drums{};
  const StrongRhythmMigrationResult result =
      migrateStrongRhythmDrums(corrupted, context, drums);
  require(result.status == StrongRhythmMigrationStatus::Applied &&
              isRhythmCompatible(technoBase,
                  ReferenceVocabulary::definitionFor(result.archetype)
                      ->archetypeId),
          "mismatched Variant escaped base Genre in end-to-end migration");
}

void testStage8FeelReachesProductionMaterialization() {
  GenreSettings settings = baseSettings(GenerativeMode::Darksynth);
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Manual);
  settings.rhythmArchetypeId = 711;  // fixed stacked-quarters fixture

  StrongRhythmMigrationContext straight{};
  straight.patternAddress = 0;
  straight.level = RealizationLevel::P1Canonical;
  DrumPatternSet straightDrums{};
  const StrongRhythmMigrationResult straightResult =
      migrateStrongRhythmDrums(settings, straight, straightDrums);
  require(straightResult.status == StrongRhythmMigrationStatus::Applied,
          "Straight production Feel failed");

  StrongRhythmMigrationContext laidBack = straight;
  laidBack.feelProfile = FeelProfileId::LaidBack;
  laidBack.feelAmount = 100;
  DrumPatternSet laidBackDrums{};
  const StrongRhythmMigrationResult laidBackResult =
      migrateStrongRhythmDrums(settings, laidBack, laidBackDrums);
  require(laidBackResult.status == StrongRhythmMigrationStatus::Applied &&
              laidBackResult.feelStatus == FeelPatternApplyStatus::Ok,
          "LaidBack production Feel failed");

  bool foundDelayedBackbeat = false;
  for (int step = 0; step < DrumPattern::kSteps; ++step) {
    const DrumStep& straightEvent = straightDrums.voices[SNARE].steps[step];
    const DrumStep& laidBackEvent = laidBackDrums.voices[SNARE].steps[step];
    if (!straightEvent.hit) continue;
    require(straightEvent.timing == 0,
            "Straight profile changed ideal grid timing");
    if (laidBackEvent.timing > 0) foundDelayedBackbeat = true;
  }
  require(foundDelayedBackbeat,
          "persisted LaidBack intent did not reach drum timing");

  DrumPatternSet sentinel = straightDrums;
  const DrumPatternSet before = sentinel;
  StrongRhythmMigrationContext invalid = laidBack;
  invalid.feelProfile = static_cast<FeelProfileId>(255);
  const StrongRhythmMigrationResult invalidResult =
      migrateStrongRhythmDrums(settings, invalid, sentinel);
  require(invalidResult.status == StrongRhythmMigrationStatus::InvalidContext &&
              std::memcmp(&sentinel, &before, sizeof(sentinel)) == 0,
          "invalid Feel context escaped transactional fallback");
}

}  // namespace

int main() {
  testEveryProductionIdIsReachable();
  testAutoIsDeterministicAndTableOrderIndependent();
  testManualIdentityAndRealizationVariation();
  testIncompatibleManualFallsBackToAuto();
  testMismatchedVariantCannotMaskGenre();
  testStage8FeelReachesProductionMaterialization();
  std::puts("Generation Stage 7C rhythm selection: OK");
  return 0;
}
