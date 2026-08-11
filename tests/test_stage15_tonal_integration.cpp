#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

SynthPattern pitchSource(int baseNote, int cycle) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    SynthStep& event = pattern.steps[step];
    event.note = static_cast<int8_t>(baseNote + (step % cycle));
    event.velocity = static_cast<uint8_t>(84 + (step % 16));
    event.accent = (step % 4u) == 0u;
    event.slide = false;
    event.ghost = false;
    event.timing = static_cast<int8_t>((step % 3u) - 1);
    event.probability = 100;
  }
  return pattern;
}

GenreSettings settingsFor(GenerativeMode mode) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = kBaseRecipeId;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(int16_t ordinal, bool tonal) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = ordinal;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = tonal;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

StepMask activeMask(const SynthPattern& pattern) {
  StepMask result = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0)
      result = static_cast<StepMask>(result | stepBit(step));
  }
  return result;
}

void assertRegister(const SynthPattern& pattern, uint8_t minimum, uint8_t maximum) {
  for (const SynthStep& step : pattern.steps) {
    if (step.note < 0) continue;
    assert(step.note >= minimum);
    assert(step.note <= maximum);
  }
}

bool pitchDiffers(const SynthPattern& a, const SynthPattern& b) {
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (a.steps[step].note != b.steps[step].note) return true;
  }
  return false;
}

void assertPatternsEqual(const SynthPattern& a, const SynthPattern& b) {
  assert(std::memcmp(&a, &b, sizeof(SynthPattern)) == 0);
}

void assertDrumsEqual(const DrumPatternSet& a, const DrumPatternSet& b) {
  assert(std::memcmp(&a, &b, sizeof(DrumPatternSet)) == 0);
}

constexpr GenerativeMode kModes[] = {
    GenerativeMode::Acid,
    GenerativeMode::Outrun,
    GenerativeMode::Darksynth,
    GenerativeMode::Electro,
    GenerativeMode::Rave,
    GenerativeMode::Reggae,
    GenerativeMode::TripHop,
    GenerativeMode::Broken,
    GenerativeMode::Chip,
    GenerativeMode::House,
    GenerativeMode::Techno,
    GenerativeMode::HipHop,
    GenerativeMode::FunkSoul,
    GenerativeMode::UkGarage,
    GenerativeMode::DrumAndBass,
    GenerativeMode::LoFi,
};

void testAllModesDeterministicAndInRegister() {
  uint32_t tonalRows = 0;
  uint32_t changedPitchRows = 0;
  uint32_t movingAcidBassRows = 0;

  for (GenerativeMode mode : kModes) {
    for (int16_t ordinal = 0; ordinal < 8; ++ordinal) {
      const GenreSettings settings = settingsFor(mode);

      DrumPatternSet legacyDrums{};
      SynthPattern legacyA = pitchSource(36, 5);
      SynthPattern legacyB = pitchSource(60, 7);
      const StrongRhythmMigrationResult legacy = migrateStrongRhythmMaterial(
          settings, contextFor(ordinal, false), legacyDrums, legacyA, legacyB);
      assert(legacy.status == StrongRhythmMigrationStatus::Applied);
      assert(!legacy.tonalMaterializationApplied);

      DrumPatternSet tonalDrums{};
      SynthPattern tonalA = pitchSource(36, 5);
      SynthPattern tonalB = pitchSource(60, 7);
      const StrongRhythmMigrationResult tonal = migrateStrongRhythmMaterial(
          settings, contextFor(ordinal, true), tonalDrums, tonalA, tonalB);
      assert(tonal.status == StrongRhythmMigrationStatus::Applied);
      assert(tonal.tonalMaterializationApplied);

      // Tonal integration owns pitch only. Rhythm/FEEL generation is identical
      // under identical context, so active semantic sites must not move.
      assertDrumsEqual(legacyDrums, tonalDrums);
      assert(activeMask(legacyA) == activeMask(tonalA));
      assert(activeMask(legacyB) == activeMask(tonalB));

      assertRegister(tonalA, 24, 47);
      assertRegister(tonalB, 48, 71);

      DrumPatternSet repeatDrums{};
      SynthPattern repeatA = pitchSource(36, 5);
      SynthPattern repeatB = pitchSource(60, 7);
      const StrongRhythmMigrationResult repeat = migrateStrongRhythmMaterial(
          settings, contextFor(ordinal, true), repeatDrums, repeatA, repeatB);
      assert(repeat.status == StrongRhythmMigrationStatus::Applied);
      assert(std::memcmp(&tonal, &repeat, sizeof(StrongRhythmMigrationResult)) == 0);
      assertDrumsEqual(tonalDrums, repeatDrums);
      assertPatternsEqual(tonalA, repeatA);
      assertPatternsEqual(tonalB, repeatB);

      ++tonalRows;
      if (pitchDiffers(legacyA, tonalA) || pitchDiffers(legacyB, tonalB))
        ++changedPitchRows;
      if (mode == GenerativeMode::Acid &&
          tonal.bassPitchContour != BassPitchContourId::RootAnchor &&
          tonal.bassPitchContour != BassPitchContourId::Auto) {
        ++movingAcidBassRows;
      }
    }
  }

  assert(tonalRows == 16u * 8u);
  assert(changedPitchRows > 0);
  assert(movingAcidBassRows > 0);
}

void testNoPitchSourceStillMaterializes() {
  const GenreSettings settings = settingsFor(GenerativeMode::Acid);
  DrumPatternSet drums{};
  SynthPattern emptyA{};
  SynthPattern emptyB{};
  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      settings, contextFor(3, true), drums, emptyA, emptyB);
  assert(result.status == StrongRhythmMigrationStatus::Applied);
  assert(result.tonalMaterializationApplied);
  assert(result.bassTonalStatus == TonalMaterializationStatus::Ok ||
         result.bassTonalStatus == TonalMaterializationStatus::ValidButEmpty);
  assertRegister(emptyA, 24, 47);
  assertRegister(emptyB, 48, 71);
}

void testInvalidTonalContextIsAtomic() {
  const GenreSettings settings = settingsFor(GenerativeMode::Acid);
  DrumPatternSet drums{};
  SynthPattern synthA = pitchSource(36, 5);
  SynthPattern synthB = pitchSource(60, 7);
  const DrumPatternSet beforeDrums = drums;
  const SynthPattern beforeA = synthA;
  const SynthPattern beforeB = synthB;

  StrongRhythmMigrationContext context = contextFor(2, true);
  context.rootPitchClass = 12;
  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      settings, context, drums, synthA, synthB);
  assert(result.status == StrongRhythmMigrationStatus::InvalidContext);
  assertDrumsEqual(drums, beforeDrums);
  assertPatternsEqual(synthA, beforeA);
  assertPatternsEqual(synthB, beforeB);
}

}  // namespace

int main() {
  testAllModesDeterministicAndInRegister();
  testNoPitchSourceStillMaterializes();
  testInvalidTonalContextIsAtomic();
  std::cout << "Stage 15 tonal integration host matrix: OK\n";
  return 0;
}
