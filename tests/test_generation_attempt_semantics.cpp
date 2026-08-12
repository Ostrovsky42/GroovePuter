#include <cassert>
#include <cstdint>
#include <cstring>

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
          event.velocity,
          static_cast<uint8_t>(event.ghost ? 1 : 0),
          static_cast<uint8_t>(event.timing),
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

      // GA-07: attemptOrdinal cannot select another rhythm archetype.
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

}  // namespace

int main() {
  assertMorphIsRetired();
  assertAttemptKeepsArchetypeAndIsDeterministic();
  return 0;
}
