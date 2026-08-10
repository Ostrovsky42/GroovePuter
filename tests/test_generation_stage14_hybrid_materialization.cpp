#include <cassert>
#include <cstdint>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

uint8_t popcount16(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count = static_cast<uint8_t>(count + (mask & 1u));
    mask = static_cast<StepMask>(mask >> 1u);
  }
  return count;
}

bool hasAnyDrumHit(const DrumPatternSet& drums) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (drums.voices[voice].steps[step].hit) return true;
    }
  }
  return false;
}

SynthPattern pitchSource(int baseNote) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(baseNote + (step % 5));
    pattern.steps[step].velocity = static_cast<uint8_t>(88 + (step % 12));
  }
  return pattern;
}

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = 0) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

GenreSettings lofiSettings(uint8_t recipe = 0) {
  GenreSettings settings = settingsFor(GenerativeMode::LoFi, recipe);
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Manual);
  settings.rhythmArchetypeId = 416;  // approved halftime_switch
  return settings;
}

StrongRhythmMigrationContext contextFor(int16_t address) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = address;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = FeelProfileId::LaidBack;
  context.feelAmount = 65;
  return context;
}

void testLoFiUsesOneChordFirstHybridSynthB() {
  bool observedMelodicFill = false;

  for (int16_t address = 0; address < 128; ++address) {
    DrumPatternSet drums{};
    SynthPattern synthA = pitchSource(36);
    SynthPattern synthB = pitchSource(60);

    const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
        lofiSettings(), contextFor(address), drums, synthA, synthB);
    assert(result.status == StrongRhythmMigrationStatus::Applied);
    assert(result.route == StrongRhythmRoute::Stage7Composition);
    assert(result.archetype == ReferenceVocabulary::Archetype::HalftimeSwitch);
    assert(result.synthBRole == SemanticSynthBRole::ChordWithMelodicFill);
    assert(result.chordRhythmApplied);
    assert(result.melodicRhythmApplied);
    assert(result.chordProjectionStatus == SemanticPatternProjectStatus::Ok);
    assert(result.melodicProjectionStatus == SemanticPatternProjectStatus::Ok);
    assert(result.chordFeelStatus == FeelInterpretStatus::Ok);
    assert(result.melodicFeelStatus == FeelInterpretStatus::Ok);
    assert((result.melodicFillOnsets & result.chordOnsets) == 0);
    assert(popcount16(result.melodicFillOnsets) <= 3);
    assert(hasAnyDrumHit(drums));

    if (result.melodicFillOnsets != 0) observedMelodicFill = true;
  }

  assert(observedMelodicFill);
}

void testEveryStage14DirectionMaterializesDrumsAcrossAddresses() {
  struct ProfileCase {
    GenerativeMode mode;
    uint8_t recipe;
  };
  constexpr ProfileCase profiles[] = {
      {GenerativeMode::House, kBaseRecipeId},
      {GenerativeMode::Techno, kBaseRecipeId},
      {GenerativeMode::HipHop, kBaseRecipeId},
      {GenerativeMode::FunkSoul, kBaseRecipeId},
      {GenerativeMode::UkGarage, kBaseRecipeId},
      {GenerativeMode::DrumAndBass, kBaseRecipeId},
      {GenerativeMode::LoFi, kBaseRecipeId},
      {GenerativeMode::LoFi, kClassicChillRecipeId},
      {GenerativeMode::LoFi, kDrunkenGrooveRecipeId},
      {GenerativeMode::LoFi, kLoFiHouseRecipeId},
      {GenerativeMode::LoFi, kMinimalSleepRecipeId},
      {GenerativeMode::HipHop, kGoldenEraRecipeId},
      {GenerativeMode::HipHop, kDustyJazzRecipeId},
  };

  for (const ProfileCase& profile : profiles) {
    for (int16_t address = 0; address < 64; ++address) {
      DrumPatternSet drums{};
      SynthPattern synthA = pitchSource(36);
      SynthPattern synthB = pitchSource(60);
      const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
          settingsFor(profile.mode, profile.recipe), contextFor(address),
          drums, synthA, synthB);
      assert(result.status == StrongRhythmMigrationStatus::Applied);
      assert(result.route == StrongRhythmRoute::Stage7Composition);
      assert(hasAnyDrumHit(drums));
    }
  }
}

void testMinimalSleepKeepsHybridAtLowBpm() {
  const GenreSettings settings = lofiSettings(kMinimalSleepRecipeId);
  const GenerationProfileView profile = generationProfileFor(settings);
  assert(profile.corridor.bpmMin == 42);
  assert(profile.corridor.suggestedBpm == 54);
  assert(profile.corridor.bpmMax == 66);
  assert(profile.secondaryRole == CompositionSecondaryRole::ChordWithMelodicFill);

  DrumPatternSet drums{};
  SynthPattern synthA = pitchSource(36);
  SynthPattern synthB = pitchSource(60);
  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      settings, contextFor(17), drums, synthA, synthB);
  assert(result.status == StrongRhythmMigrationStatus::Applied);
  assert(result.corridor.suggestedBpm == 54);
  assert(result.synthBRole == SemanticSynthBRole::ChordWithMelodicFill);
  assert(hasAnyDrumHit(drums));
}

void testRestHeavyCanBeIntentionallyEmptyOnSlowBarCoordinate() {
  MelodicMotifRequest request{};
  request.requestedRhythm = MelodicRhythmId::RestHeavy;
  request.requestedShape = MotifShapeId::SourceOrder;
  request.family = RhythmFamily::Breakbeat;
  request.archetypeId = 416;
  request.allowEmptyBar = true;
  request.barOrdinal = 1;
  request.generation.projectSeed = 14;
  request.generation.phraseOrdinal = 1;
  const MelodicMotifResult result = realizeMelodicMotif(request);
  assert(result.status == MelodicMotifStatus::ValidButEmpty);
  assert(result.plan.onsets == 0);
}

}  // namespace

int main() {
  testLoFiUsesOneChordFirstHybridSynthB();
  testEveryStage14DirectionMaterializesDrumsAcrossAddresses();
  testMinimalSleepKeepsHybridAtLowBpm();
  testRestHeavyCanBeIntentionallyEmptyOnSlowBarCoordinate();
  return 0;
}
