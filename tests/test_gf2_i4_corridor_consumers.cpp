// GF2-I4 — musical corridor consumers.
//
// RED contract written before production changes.
//
// densityMin/densityMax are a profile-level 0..16 activity corridor. They do
// not own topology and they do not post-process material. The selected point is
// projected into the already-authoritative RhythmArchetype::DensityContract,
// and RhythmPhraseRealizer remains the sole structural-density executor.
//
// gridSteps is different: Groove Vocabulary Core v1 is normatively 4/4 x 16
// structural steps. 8/32 are not current musical capacity, so production
// profiles must not advertise them as valid generation resolution.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/materialization/pattern_materializer.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr RhythmArchetypeId kControlledArchetypeId = 415;  // sparse_fast_break
constexpr uint16_t kPhraseIdentity = 17;
constexpr uint8_t kMinStructuralOnsetSpread = 4;
constexpr uint8_t kMinMaterializedDrumStructuralSpread = 3;

int g_failures = 0;

void expect(const char* label, bool condition) {
  if (condition) {
    std::printf("%-66s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-66s FAIL\n", label);
  ++g_failures;
}

uint8_t popcount16(StepMask value) {
  uint8_t count = 0;
  while (value != 0u) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

uint16_t structuralOnsets(const RhythmPhrasePlan& plan) {
  uint16_t count = 0;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      count = static_cast<uint16_t>(
          count + popcount16(plan.bars[bar].roles[role].structural));
    }
  }
  return count;
}

uint8_t corridorCenter(const GenerationCorridor& corridor) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(corridor.densityMin) + corridor.densityMax + 1u) /
      2u);
}

GenreSettings settingsFor(GenerativeMode mode) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = kBaseRecipeId;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Manual);
  value.rhythmArchetypeId = kControlledArchetypeId;
  return value;
}

GenerationContext controlledGeneration() {
  GenerationContext value{};
  value.projectSeed = 0x49344445u;
  value.phraseOrdinal = kPhraseIdentity;
  return value;
}

StrongRhythmMigrationContext migrationContext() {
  StrongRhythmMigrationContext value{};
  value.patternAddress = 17;
  value.level = RealizationLevel::P2Variation;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  return value;
}

const RhythmArchetype* controlledArchetype() {
  const auto* definition =
      ReferenceVocabulary::definitionForId(kControlledArchetypeId);
  return definition == nullptr ? nullptr
                               : ReferenceVocabulary::archetypeFor(definition->key);
}

bool compatibilityContains(const GenerationProfileView& profile,
                           RhythmArchetypeId archetypeId) {
  for (uint8_t i = 0; i < profile.rhythms.count; ++i) {
    if (profile.rhythms.candidates[i].archetypeId == archetypeId &&
        profile.rhythms.candidates[i].weight != 0) {
      return true;
    }
  }
  return false;
}

RhythmRealizationResult realizeAtTarget(uint8_t target) {
  RhythmRealizationRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = kControlledArchetypeId;
  request.phraseBars = 1;
  request.level = RealizationLevel::P2Variation;
  request.generation = controlledGeneration();
  request.structuralDensityTarget = target;
  return realizeRhythmPhrase(request);
}

PatternMaterializationDiagnostics materializeDiagnostics(
    const RhythmPhrasePlan& plan) {
  const RhythmRoleMask ignored = static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::BassRhythm) |
      rhythmRoleBit(RhythmRole::ChordRhythm) |
      rhythmRoleBit(RhythmRole::MelodicRhythm));
  const PatternMaterializerBinding binding = standardDrumPatternBinding(ignored);
  MaterializedPatterns material{};
  PatternMaterializationDiagnostics diagnostics{};
  const PatternMaterializeStatus status =
      materializeRhythmPattern(plan, binding, material, &diagnostics);
  expect("controlled rhythm plan materializes to physical drum output",
         status == PatternMaterializeStatus::Ok);
  return diagnostics;
}

void testGridIsIntentionalFixedCapacity() {
  bool allShippedProfilesAre16 = true;
  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    const GenerativeMode genre = static_cast<GenerativeMode>(mode);
    const uint8_t count = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < count; ++ordinal) {
      GenreRecipeId recipe = kBaseRecipeId;
      if (!availableRecipeAt(genre, ordinal, recipe)) {
        allShippedProfilesAre16 = false;
        continue;
      }
      GenreSettings settings{};
      settings.generativeMode = mode;
      settings.recipe = recipe;
      const GenerationProfileView profile = generationProfileFor(settings);
      if (profile.rhythms.candidates == nullptr ||
          profile.corridor.gridSteps != kStepsPerBar) {
        allShippedProfilesAre16 = false;
      }
    }
  }
  expect("all shipped generation profiles use the Core-v1 16-step grid",
         allShippedProfilesAre16);

  GenerationProfileView profile =
      generationProfileFor(settingsFor(GenerativeMode::LoFi));
  expect("fixture: shipped Lo-Fi profile is valid",
         isValidGenerationProfile(profile));
  profile.corridor.gridSteps = 8;
  expect("8-step structural generation is rejected as unsupported capacity",
         !isValidGenerationProfile(profile));
  profile.corridor.gridSteps = 32;
  expect("32-step structural generation is rejected as unsupported capacity",
         !isValidGenerationProfile(profile));
}

void testDensityMagnitudeAtShippedDefaults() {
  const GenerationProfileView sparseProfile =
      generationProfileFor(settingsFor(GenerativeMode::LoFi));
  const GenerationProfileView denseProfile =
      generationProfileFor(settingsFor(GenerativeMode::DrumAndBass));
  const RhythmArchetype* archetype = controlledArchetype();

  expect("fixture: controlled archetype exists", archetype != nullptr);
  if (archetype == nullptr) return;
  expect("fixture: Lo-Fi can select sparse_fast_break",
         compatibilityContains(sparseProfile, kControlledArchetypeId));
  expect("fixture: DnB can select sparse_fast_break",
         compatibilityContains(denseProfile, kControlledArchetypeId));
  expect("fixture: Lo-Fi shipped density corridor is 2..8",
         sparseProfile.corridor.densityMin == 2 &&
             sparseProfile.corridor.densityMax == 8);
  expect("fixture: DnB shipped density corridor is 7..15",
         denseProfile.corridor.densityMin == 7 &&
             denseProfile.corridor.densityMax == 15);

  const uint8_t sparseTarget = projectStructuralDensityTarget(
      *archetype, corridorCenter(sparseProfile.corridor));
  const uint8_t denseTarget = projectStructuralDensityTarget(
      *archetype, corridorCenter(denseProfile.corridor));

  std::printf("  density centers sparse=%u dense=%u -> targets=%u/%u\n",
              static_cast<unsigned>(corridorCenter(sparseProfile.corridor)),
              static_cast<unsigned>(corridorCenter(denseProfile.corridor)),
              static_cast<unsigned>(sparseTarget),
              static_cast<unsigned>(denseTarget));
  expect("profile corridor projects inside archetype legal density bounds",
         sparseTarget >= archetype->density.structuralMin &&
             sparseTarget <= archetype->density.structuralMax &&
             denseTarget >= archetype->density.structuralMin &&
             denseTarget <= archetype->density.structuralMax);

  const RhythmRealizationResult sparse = realizeAtTarget(sparseTarget);
  const RhythmRealizationResult dense = realizeAtTarget(denseTarget);
  expect("sparse controlled realization succeeds",
         sparse.status == RealizationStatus::Ok ||
             sparse.status == RealizationStatus::ValidButSparse);
  expect("dense controlled realization succeeds",
         dense.status == RealizationStatus::Ok ||
             dense.status == RealizationStatus::ValidButSparse);
  if ((sparse.status != RealizationStatus::Ok &&
       sparse.status != RealizationStatus::ValidButSparse) ||
      (dense.status != RealizationStatus::Ok &&
       dense.status != RealizationStatus::ValidButSparse)) {
    return;
  }

  const uint16_t sparseStructural = structuralOnsets(sparse.plan);
  const uint16_t denseStructural = structuralOnsets(dense.plan);
  const uint16_t structuralSpread = denseStructural >= sparseStructural
      ? static_cast<uint16_t>(denseStructural - sparseStructural)
      : 0;
  std::printf("  structural onsets sparse=%u dense=%u spread=%u\n",
              static_cast<unsigned>(sparseStructural),
              static_cast<unsigned>(denseStructural),
              static_cast<unsigned>(structuralSpread));
  expect("density magnitude: >=4 structural onsets/bar at shipped defaults",
         structuralSpread >= kMinStructuralOnsetSpread);

  const PatternMaterializationDiagnostics sparseMaterial =
      materializeDiagnostics(sparse.plan);
  const PatternMaterializationDiagnostics denseMaterial =
      materializeDiagnostics(dense.plan);
  const uint16_t materializedSpread =
      denseMaterial.structuralEvents >= sparseMaterial.structuralEvents
          ? static_cast<uint16_t>(denseMaterial.structuralEvents -
                                  sparseMaterial.structuralEvents)
          : 0;
  std::printf("  materialized drum structural events sparse=%u dense=%u spread=%u\n",
              static_cast<unsigned>(sparseMaterial.structuralEvents),
              static_cast<unsigned>(denseMaterial.structuralEvents),
              static_cast<unsigned>(materializedSpread));
  expect("density magnitude: >=3 materialized structural drum hits",
         materializedSpread >= kMinMaterializedDrumStructuralSpread);

  const uint8_t kick = static_cast<uint8_t>(RhythmRole::Kick);
  const uint8_t backbeat = static_cast<uint8_t>(RhythmRole::Backbeat);
  const LaneGrammar* kickLane = nullptr;
  const LaneGrammar* backbeatLane = nullptr;
  for (uint8_t i = 0; i < archetype->laneCount; ++i) {
    if (archetype->lanes[i].role == RhythmRole::Kick)
      kickLane = &archetype->lanes[i];
    if (archetype->lanes[i].role == RhythmRole::Backbeat)
      backbeatLane = &archetype->lanes[i];
  }
  const bool sparseAnchors = kickLane != nullptr && backbeatLane != nullptr &&
      (kickLane->canonicalAnchors & ~sparse.plan.bars[0].roles[kick].structural) == 0 &&
      (backbeatLane->canonicalAnchors &
       ~sparse.plan.bars[0].roles[backbeat].structural) == 0;
  const bool denseAnchors = kickLane != nullptr && backbeatLane != nullptr &&
      (kickLane->canonicalAnchors & ~dense.plan.bars[0].roles[kick].structural) == 0 &&
      (backbeatLane->canonicalAnchors &
       ~dense.plan.bars[0].roles[backbeat].structural) == 0;
  expect("density changes activity without deleting canonical primary anchors",
         sparseAnchors && denseAnchors);
}

void testFrozenSelectionIsTheSingleArbitrationPoint() {
  const RhythmArchetype* archetype = controlledArchetype();
  if (archetype == nullptr) return;
  const GenreSettings sparseSettings = settingsFor(GenerativeMode::LoFi);
  const GenreSettings denseSettings = settingsFor(GenerativeMode::DrumAndBass);
  const GenerationProfileView sparseProfile = generationProfileFor(sparseSettings);
  const GenerationProfileView denseProfile = generationProfileFor(denseSettings);
  const uint8_t expectedSparse = projectStructuralDensityTarget(
      *archetype, corridorCenter(sparseProfile.corridor));
  const uint8_t expectedDense = projectStructuralDensityTarget(
      *archetype, corridorCenter(denseProfile.corridor));

  StrongRhythmFrozenSelection sparse{};
  StrongRhythmFrozenSelection dense{};
  const StrongRhythmMigrationContext context = migrationContext();
  const StrongRhythmMigrationResult sparseResult =
      resolveStrongRhythmFrozenSelection(
          sparseSettings, context, kPhraseIdentity, sparse);
  const StrongRhythmMigrationResult denseResult =
      resolveStrongRhythmFrozenSelection(
          denseSettings, context, kPhraseIdentity, dense);
  expect("sparse frozen selection resolves",
         sparseResult.status == StrongRhythmMigrationStatus::Applied);
  expect("dense frozen selection resolves",
         denseResult.status == StrongRhythmMigrationStatus::Applied);
  expect("frozen selection stores the sparse profile density arbitration",
         sparse.structuralDensityTarget == expectedSparse);
  expect("frozen selection stores the dense profile density arbitration",
         dense.structuralDensityTarget == expectedDense);
}

void testNoDensityIntentPreservesLegacyPreferred() {
  const RhythmArchetype* archetype = controlledArchetype();
  if (archetype == nullptr) return;

  RhythmRealizationRequest legacy{};
  legacy.catalog = &ReferenceVocabulary::catalog();
  legacy.archetypeId = kControlledArchetypeId;
  legacy.phraseBars = 1;
  legacy.level = RealizationLevel::P2Variation;
  legacy.generation = controlledGeneration();

  RhythmRealizationRequest explicitPreferred = legacy;
  explicitPreferred.structuralDensityTarget =
      archetype->density.structuralPreferred;

  const RhythmRealizationResult a = realizeRhythmPhrase(legacy);
  const RhythmRealizationResult b = realizeRhythmPhrase(explicitPreferred);
  expect("legacy no-intent realization remains successful",
         a.status == RealizationStatus::Ok ||
             a.status == RealizationStatus::ValidButSparse);
  expect("explicit preferred realization remains successful",
         b.status == RealizationStatus::Ok ||
             b.status == RealizationStatus::ValidButSparse);
  expect("no density intent is byte-identical to archetype preferred behavior",
         std::memcmp(&a, &b, sizeof(a)) == 0);
}

}  // namespace

int main() {
  testGridIsIntentionalFixedCapacity();
  testDensityMagnitudeAtShippedDefaults();
  testFrozenSelectionIsTheSingleArbitrationPoint();
  testNoDensityIntentPreservesLegacyPreferred();

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I4 corridor consumers: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("GF2-I4 corridor consumers: PASS\n");
  return 0;
}
