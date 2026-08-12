#include <cassert>
#include <cstdint>

#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

uint32_t drumFingerprint(const DrumPatternSet& drums) {
  uint32_t hash = 2166136261u;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = drums.voices[voice].steps[step];
      const uint8_t bytes[] = {
          static_cast<uint8_t>(event.hit ? 1 : 0),
          static_cast<uint8_t>(event.accent ? 1 : 0),
          event.velocity,
          static_cast<uint8_t>(event.timing),
          event.fx,
          event.fxParam,
          event.probability,
      };
      for (uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
      }
    }
  }
  return hash;
}

uint32_t synthFingerprint(const SynthPattern& pattern) {
  uint32_t hash = 2166136261u;
  for (const SynthStep& event : pattern.steps) {
    const uint8_t bytes[] = {
        static_cast<uint8_t>(event.note),
        static_cast<uint8_t>(event.slide ? 1 : 0),
        static_cast<uint8_t>(event.accent ? 1 : 0),
        static_cast<uint8_t>(event.ghost ? 1 : 0),
        event.velocity,
        static_cast<uint8_t>(event.timing),
        event.fx,
        event.fxParam,
        event.probability,
    };
    for (uint8_t byte : bytes) {
      hash ^= byte;
      hash *= 16777619u;
    }
  }
  return hash;
}

uint8_t synthNoteCount(const SynthPattern& pattern) {
  uint8_t result = 0;
  for (const SynthStep& event : pattern.steps) {
    if (event.note >= 0) ++result;
  }
  return result;
}

uint32_t materialFingerprint(const DrumPatternSet& drums,
                             const SynthPattern& synthA,
                             const SynthPattern& synthB) {
  uint32_t hash = drumFingerprint(drums);
  hash ^= synthFingerprint(synthA);
  hash *= 16777619u;
  hash ^= synthFingerprint(synthB);
  hash *= 16777619u;
  return hash;
}

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.morphTarget = 0;
  settings.morphAmount = 0;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(int address, uint32_t attempt) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(address);
  context.level = RealizationLevel::P2Variation;
  context.generationAttemptOrdinal = attempt;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  return context;
}

void assertMorphIsRetired() {
  GenreSettings clean = settingsFor(GenerativeMode::Acid, 6);
  GenreSettings historical = clean;
  historical.morphTarget = 11;
  historical.morphAmount = 255;

  assert(selectStrongRhythmRoute(clean) == selectStrongRhythmRoute(historical));

  DrumPatternSet cleanDrums{};
  DrumPatternSet historicalDrums{};
  const auto cleanResult = migrateStrongRhythmDrums(
      clean, contextFor(17, 0), cleanDrums);
  const auto historicalResult = migrateStrongRhythmDrums(
      historical, contextFor(17, 0), historicalDrums);

  assert(cleanResult.status == StrongRhythmMigrationStatus::Applied);
  assert(historicalResult.status == StrongRhythmMigrationStatus::Applied);
  assert(cleanResult.archetype == historicalResult.archetype);
  assert(cleanResult.bassRhythmId == historicalResult.bassRhythmId);
  assert(cleanResult.chordRhythmId == historicalResult.chordRhythmId);
  assert(cleanResult.progressionId == historicalResult.progressionId);
  assert(cleanResult.melodicRhythmId == historicalResult.melodicRhythmId);
  assert(cleanResult.motifShapeId == historicalResult.motifShapeId);
  assert(drumFingerprint(cleanDrums) == drumFingerprint(historicalDrums));
}

void assertAttemptKeepsArchetypeAndIsDeterministic() {
  bool observedBoundedVariation = false;
  const GenerativeMode modes[] = {
      GenerativeMode::House,
      GenerativeMode::Techno,
      GenerativeMode::Acid,
      GenerativeMode::Electro,
      GenerativeMode::HipHop,
      GenerativeMode::FunkSoul,
      GenerativeMode::UkGarage,
      GenerativeMode::Broken,
  };

  for (GenerativeMode mode : modes) {
    GenreSettings settings = settingsFor(mode, kBaseRecipeId);
    for (int address = 0; address < 32; ++address) {
      DrumPatternSet attempt0{};
      DrumPatternSet attempt1a{};
      DrumPatternSet attempt1b{};
      const auto zero = migrateStrongRhythmDrums(
          settings, contextFor(address, 0), attempt0);
      const auto oneA = migrateStrongRhythmDrums(
          settings, contextFor(address, 1), attempt1a);
      const auto oneB = migrateStrongRhythmDrums(
          settings, contextFor(address, 1), attempt1b);

      if (zero.status != StrongRhythmMigrationStatus::Applied) continue;
      assert(oneA.status == StrongRhythmMigrationStatus::Applied);
      assert(oneB.status == StrongRhythmMigrationStatus::Applied);

      // GA-07: attemptOrdinal cannot select another rhythm archetype or
      // composition identity.
      assert(zero.archetype == oneA.archetype);
      assert(zero.bassRhythmId == oneA.bassRhythmId);
      assert(zero.chordRhythmId == oneA.chordRhythmId);
      assert(zero.progressionId == oneA.progressionId);
      assert(zero.melodicRhythmId == oneA.melodicRhythmId);
      assert(zero.motifShapeId == oneA.motifShapeId);

      // A specific accepted attempt is deterministic even if publication later
      // gets cancelled or superseded.
      assert(drumFingerprint(attempt1a) == drumFingerprint(attempt1b));

      if (drumFingerprint(attempt0) != drumFingerprint(attempt1a))
        observedBoundedVariation = true;
    }
  }

  assert(observedBoundedVariation);
}

void assertReachableFullMaterialRerollsDiffer() {
  struct Profile {
    GenerativeMode mode;
    uint8_t recipe;
  };
  constexpr Profile profiles[] = {
      {GenerativeMode::Acid, 0}, {GenerativeMode::Outrun, 0},
      {GenerativeMode::Darksynth, 0}, {GenerativeMode::Electro, 0},
      {GenerativeMode::Rave, 0}, {GenerativeMode::Reggae, 0},
      {GenerativeMode::TripHop, 0}, {GenerativeMode::Broken, 0},
      {GenerativeMode::Chip, 0}, {GenerativeMode::House, 0},
      {GenerativeMode::Techno, 0}, {GenerativeMode::HipHop, 0},
      {GenerativeMode::FunkSoul, 0}, {GenerativeMode::UkGarage, 0},
      {GenerativeMode::DrumAndBass, 0}, {GenerativeMode::LoFi, 0},
      {GenerativeMode::Broken, 1}, {GenerativeMode::Broken, 2},
      {GenerativeMode::Broken, 3}, {GenerativeMode::Rave, 4},
      {GenerativeMode::Reggae, 5}, {GenerativeMode::Acid, 6},
      {GenerativeMode::Acid, 7}, {GenerativeMode::Broken, 8},
      {GenerativeMode::Broken, 9}, {GenerativeMode::Reggae, 10},
      {GenerativeMode::Reggae, 11},
      {GenerativeMode::LoFi, kClassicChillRecipeId},
      {GenerativeMode::LoFi, kDrunkenGrooveRecipeId},
      {GenerativeMode::LoFi, kLoFiHouseRecipeId},
      {GenerativeMode::LoFi, kMinimalSleepRecipeId},
      {GenerativeMode::HipHop, kGoldenEraRecipeId},
      {GenerativeMode::HipHop, kDustyJazzRecipeId},
  };
  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };

  for (const Profile& profile : profiles) {
    const GenreSettings settings = settingsFor(profile.mode, profile.recipe);
    for (RealizationLevel level : levels) {
      for (int address = 0; address < 32; ++address) {
        StrongRhythmMigrationContext zeroContext = contextFor(address, 0);
        StrongRhythmMigrationContext oneContext = contextFor(address, 1);
        zeroContext.level = level;
        oneContext.level = level;
        zeroContext.tonalMaterializationEnabled = true;
        oneContext.tonalMaterializationEnabled = true;

        DrumPatternSet zeroDrums{};
        DrumPatternSet oneDrums{};
        SynthPattern zeroA{};
        SynthPattern zeroB{};
        SynthPattern oneA{};
        SynthPattern oneB{};
        const auto zero = migrateStrongRhythmMaterial(
            settings, zeroContext, zeroDrums, zeroA, zeroB);
        oneDrums = zeroDrums;
        oneA = zeroA;
        oneB = zeroB;
        const auto one = migrateStrongRhythmMaterial(
            settings, oneContext, oneDrums, oneA, oneB);
        assert(zero.status == StrongRhythmMigrationStatus::Applied);
        assert(one.status == StrongRhythmMigrationStatus::Applied);
        assert(zero.archetype == one.archetype);
        assert(zero.bassRhythmId == one.bassRhythmId);
        assert(zero.chordRhythmId == one.chordRhythmId);
        assert(zero.progressionId == one.progressionId);
        assert(zero.melodicRhythmId == one.melodicRhythmId);
        assert(zero.motifShapeId == one.motifShapeId);
        assert(materialFingerprint(zeroDrums, zeroA, zeroB) !=
               materialFingerprint(oneDrums, oneA, oneB));

        DrumPatternSet synthOnlyDrums = zeroDrums;
        SynthPattern synthOnlyA = zeroA;
        SynthPattern synthOnlyB = zeroB;
        const uint32_t preservedDrums = drumFingerprint(synthOnlyDrums);
        const auto synthOnly = migrateStrongRhythmSynths(
            settings,
            oneContext,
            synthOnlyDrums,
            synthOnlyA,
            synthOnlyB);
        assert(synthOnly.status == StrongRhythmMigrationStatus::Applied);
        assert(synthOnly.archetype == zero.archetype);
        assert(synthOnly.bassRhythmId == zero.bassRhythmId);
        assert(synthOnly.chordRhythmId == zero.chordRhythmId);
        assert(synthOnly.progressionId == zero.progressionId);
        assert(synthOnly.melodicRhythmId == zero.melodicRhythmId);
        assert(synthOnly.motifShapeId == zero.motifShapeId);
        assert(drumFingerprint(synthOnlyDrums) == preservedDrums);
        if (synthNoteCount(zeroA) != 0)
          assert(synthFingerprint(synthOnlyA) != synthFingerprint(zeroA));
        if (synthNoteCount(zeroB) != 0)
          assert(synthFingerprint(synthOnlyB) != synthFingerprint(zeroB));
      }
    }
  }
}

}  // namespace

int main() {
  assertMorphIsRetired();
  assertAttemptKeepsArchetypeAndIsDeterministic();
  assertReachableFullMaterialRerollsDiffer();
  return 0;
}
