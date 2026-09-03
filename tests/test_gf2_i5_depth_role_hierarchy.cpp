// GF2-I5 — DEPTH / role-hierarchy characterization.
//
// Final contract after executable RED characterization:
// - P1/P2/P3 is a causal realization/transformation-magnitude axis.
// - changing only RealizationLevel does not change profile-selected Synth-B
//   role identity, role admission, or materialized Synth-B activity in the
//   measured shipped families.
// - profile.secondaryRole independently owns Synth-B role participation.
//
// I5 therefore records negative capacity for role hierarchy via DEPTH without
// changing production semantics or rewriting the frozen I3 trajectory contract.

#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr RhythmArchetypeId kControlledArchetypeId = 415;  // sparse_fast_break
constexpr uint16_t kPhraseIdentity = 17;
constexpr uint8_t kMinDensityStructuralSpread = 4;

int g_failures = 0;

void expect(const char* label, bool condition) {
  if (condition) {
    std::printf("%-72s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-72s FAIL\n", label);
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

uint8_t synthActivity(const SynthPattern& pattern) {
  uint8_t count = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) ++count;
  }
  return count;
}

SynthPattern pitchSource(int baseNote) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(baseNote + (step % 5));
    pattern.steps[step].velocity = static_cast<uint8_t>(88 + (step % 12));
  }
  return pattern;
}

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = kBaseRecipeId) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = recipe;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
  return value;
}

GenreSettings controlledRhythmSettings(GenerativeMode mode) {
  GenreSettings value = settingsFor(mode);
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Manual);
  value.rhythmArchetypeId = kControlledArchetypeId;
  return value;
}

StrongRhythmMigrationContext migrationContext(RealizationLevel level,
                                               int16_t address = 17) {
  StrongRhythmMigrationContext value{};
  value.patternAddress = address;
  value.level = level;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  return value;
}

uint8_t corridorCenter(const GenerationCorridor& corridor) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(corridor.densityMin) + corridor.densityMax + 1u) /
      2u);
}

struct RhythmMetrics {
  bool ready = false;
  uint16_t structural = 0;
  uint16_t secondary = 0;
  uint16_t ghosts = 0;
  uint16_t total = 0;
  uint32_t topologyHash = 2166136261u;
  bool primaryAnchorsPreserved = true;
};

void hashMask(uint32_t& hash, StepMask mask) {
  hash = (hash ^ static_cast<uint8_t>(mask >> 8u)) * 16777619u;
  hash = (hash ^ static_cast<uint8_t>(mask & 0xFFu)) * 16777619u;
}

RhythmMetrics rhythmMetricsFor(const GenerationProfileView& profile,
                               RealizationLevel level) {
  RhythmMetrics metrics{};
  const auto* definition =
      ReferenceVocabulary::definitionForId(kControlledArchetypeId);
  const RhythmArchetype* archetype = definition == nullptr
      ? nullptr
      : ReferenceVocabulary::archetypeFor(definition->key);
  if (archetype == nullptr) return metrics;

  RhythmRealizationRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = kControlledArchetypeId;
  request.phraseBars = 1;
  request.level = level;
  request.generation.projectSeed = 0x49344445u;
  request.generation.phraseOrdinal = kPhraseIdentity;
  request.structuralDensityTarget = projectStructuralDensityTarget(
      *archetype, corridorCenter(profile.corridor));

  const RhythmRealizationResult realization = realizeRhythmPhrase(request);
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    return metrics;
  }

  metrics.ready = true;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    const RoleRhythmPlan& lane = realization.plan.bars[0].roles[role];
    metrics.structural = static_cast<uint16_t>(
        metrics.structural + popcount16(lane.structural));
    metrics.secondary = static_cast<uint16_t>(
        metrics.secondary + popcount16(lane.secondary));
    metrics.ghosts = static_cast<uint16_t>(
        metrics.ghosts + popcount16(lane.ghosts));
    hashMask(metrics.topologyHash, lane.structural);
    hashMask(metrics.topologyHash, lane.secondary);
    hashMask(metrics.topologyHash, lane.ghosts);
  }
  metrics.total = static_cast<uint16_t>(
      metrics.structural + metrics.secondary + metrics.ghosts);

  for (uint8_t laneIndex = 0; laneIndex < archetype->laneCount; ++laneIndex) {
    const LaneGrammar& grammar = archetype->lanes[laneIndex];
    const uint8_t role = static_cast<uint8_t>(grammar.role);
    const StepMask anchors = static_cast<StepMask>(
        grammar.immutableAnchors | grammar.canonicalAnchors);
    if ((anchors & ~realization.plan.bars[0].roles[role].structural) != 0) {
      metrics.primaryAnchorsPreserved = false;
    }
  }
  return metrics;
}

uint16_t transformationActivity(const RhythmMetrics& metrics) {
  return static_cast<uint16_t>(metrics.secondary + metrics.ghosts);
}

struct SynthBObservation {
  bool ready = false;
  CompositionSecondaryRole selectedRole = CompositionSecondaryRole::Count;
  SemanticSynthBRole materializedRole = SemanticSynthBRole::Count;
  bool chordApplied = false;
  bool melodicApplied = false;
  uint8_t activeEvents = 0;
  uint8_t chordEvents = 0;
  uint8_t melodicFillEvents = 0;
};

SynthBObservation synthBObservation(const GenreSettings& settings,
                                    RealizationLevel level,
                                    int16_t address) {
  SynthBObservation observation{};
  const GenerationProfileView profile = generationProfileFor(settings);
  observation.selectedRole = profile.secondaryRole;

  DrumPatternSet drums{};
  SynthPattern synthA = pitchSource(36);
  SynthPattern synthB = pitchSource(60);
  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      settings, migrationContext(level, address), drums, synthA, synthB);
  if (result.status != StrongRhythmMigrationStatus::Applied) return observation;

  observation.ready = true;
  observation.materializedRole = result.synthBRole;
  observation.chordApplied = result.chordRhythmApplied;
  observation.melodicApplied = result.melodicRhythmApplied;
  observation.activeEvents = synthActivity(synthB);
  observation.chordEvents = popcount16(result.chordOnsets);
  observation.melodicFillEvents = popcount16(result.melodicFillOnsets);
  return observation;
}

uint8_t participationSignature(const SynthBObservation& observation) {
  return static_cast<uint8_t>((observation.chordApplied ? 1u : 0u) |
                              (observation.melodicApplied ? 2u : 0u));
}

bool sameMaterializedActivity(const SynthBObservation& lhs,
                              const SynthBObservation& rhs) {
  return lhs.activeEvents == rhs.activeEvents &&
      lhs.chordEvents == rhs.chordEvents &&
      lhs.melodicFillEvents == rhs.melodicFillEvents;
}

SemanticSynthBRole expectedSemanticRole(CompositionSecondaryRole role) {
  switch (role) {
    case CompositionSecondaryRole::Chord:
      return SemanticSynthBRole::Chord;
    case CompositionSecondaryRole::Melodic:
      return SemanticSynthBRole::Melodic;
    case CompositionSecondaryRole::ChordWithMelodicFill:
      return SemanticSynthBRole::ChordWithMelodicFill;
    case CompositionSecondaryRole::Count:
      return SemanticSynthBRole::Count;
  }
  return SemanticSynthBRole::Count;
}

void printRhythmMetrics(const char* family, const char* level,
                        const RhythmMetrics& metrics) {
  std::printf(
      "  %-8s %-2s structural=%u secondary=%u ghosts=%u total=%u topology=%08x anchors=%s\n",
      family, level,
      static_cast<unsigned>(metrics.structural),
      static_cast<unsigned>(metrics.secondary),
      static_cast<unsigned>(metrics.ghosts),
      static_cast<unsigned>(metrics.total),
      static_cast<unsigned>(metrics.topologyHash),
      metrics.primaryAnchorsPreserved ? "preserved" : "changed");
}

void printSynthB(const char* family, const char* level,
                 const SynthBObservation& observation) {
  std::printf(
      "  %-8s %-2s selected_role=%u materialized_role=%u participation=%u synthB_events=%u chord_onsets=%u melodic_fill=%u\n",
      family, level,
      static_cast<unsigned>(observation.selectedRole),
      static_cast<unsigned>(observation.materializedRole),
      static_cast<unsigned>(participationSignature(observation)),
      static_cast<unsigned>(observation.activeEvents),
      static_cast<unsigned>(observation.chordEvents),
      static_cast<unsigned>(observation.melodicFillEvents));
}

void testCurrentDepthIsCausalRealizationMagnitude() {
  const GenerationProfileView lofi =
      generationProfileFor(controlledRhythmSettings(GenerativeMode::LoFi));
  const GenerationProfileView dnb =
      generationProfileFor(controlledRhythmSettings(GenerativeMode::DrumAndBass));

  const RhythmMetrics lofiP1 = rhythmMetricsFor(lofi, RealizationLevel::P1Canonical);
  const RhythmMetrics lofiP2 = rhythmMetricsFor(lofi, RealizationLevel::P2Variation);
  const RhythmMetrics lofiP3 = rhythmMetricsFor(lofi, RealizationLevel::P3Transformation);
  const RhythmMetrics dnbP1 = rhythmMetricsFor(dnb, RealizationLevel::P1Canonical);
  const RhythmMetrics dnbP2 = rhythmMetricsFor(dnb, RealizationLevel::P2Variation);
  const RhythmMetrics dnbP3 = rhythmMetricsFor(dnb, RealizationLevel::P3Transformation);

  printRhythmMetrics("Lo-Fi", "P1", lofiP1);
  printRhythmMetrics("Lo-Fi", "P2", lofiP2);
  printRhythmMetrics("Lo-Fi", "P3", lofiP3);
  printRhythmMetrics("DnB", "P1", dnbP1);
  printRhythmMetrics("DnB", "P2", dnbP2);
  printRhythmMetrics("DnB", "P3", dnbP3);

  expect("Lo-Fi P1/P2/P3 controlled rhythm realizations succeed",
         lofiP1.ready && lofiP2.ready && lofiP3.ready);
  expect("DnB P1/P2/P3 controlled rhythm realizations succeed",
         dnbP1.ready && dnbP2.ready && dnbP3.ready);
  expect("Lo-Fi primary structural activity is stable across DEPTH",
         lofiP1.structural == lofiP2.structural &&
             lofiP2.structural == lofiP3.structural);
  expect("DnB primary structural activity is stable across DEPTH",
         dnbP1.structural == dnbP2.structural &&
             dnbP2.structural == dnbP3.structural);
  expect("Lo-Fi DEPTH increases optional/ghost transformation activity",
         transformationActivity(lofiP1) < transformationActivity(lofiP2) &&
             transformationActivity(lofiP2) < transformationActivity(lofiP3));
  expect("DnB DEPTH increases optional/ghost transformation activity",
         transformationActivity(dnbP1) < transformationActivity(dnbP2) &&
             transformationActivity(dnbP2) < transformationActivity(dnbP3));
  expect("Lo-Fi DEPTH changes rhythm realization topology",
         lofiP1.topologyHash != lofiP2.topologyHash &&
             lofiP2.topologyHash != lofiP3.topologyHash);
  expect("DnB DEPTH changes rhythm realization topology",
         dnbP1.topologyHash != dnbP2.topologyHash &&
             dnbP2.topologyHash != dnbP3.topologyHash);
  expect("P1/P2/P3 preserve canonical primary anchors in both families",
         lofiP1.primaryAnchorsPreserved && lofiP2.primaryAnchorsPreserved &&
             lofiP3.primaryAnchorsPreserved && dnbP1.primaryAnchorsPreserved &&
             dnbP2.primaryAnchorsPreserved && dnbP3.primaryAnchorsPreserved);

  const uint16_t densitySpread = dnbP2.structural >= lofiP2.structural
      ? static_cast<uint16_t>(dnbP2.structural - lofiP2.structural)
      : 0;
  std::printf("  I4 density witness at P2: Lo-Fi=%u DnB=%u spread=%u\n",
              static_cast<unsigned>(lofiP2.structural),
              static_cast<unsigned>(dnbP2.structural),
              static_cast<unsigned>(densitySpread));
  expect("I4 density sensitivity remains causal while DEPTH varies independently",
         densitySpread >= kMinDensityStructuralSpread);

  const TrajectoryId developP2 = phraseTrajectoryForLaw(
      PhraseEvolutionLawId::DevelopReturn, RealizationLevel::P2Variation);
  const TrajectoryId developP3 = phraseTrajectoryForLaw(
      PhraseEvolutionLawId::DevelopReturn, RealizationLevel::P3Transformation);
  const TrajectoryId sparseP2 = phraseTrajectoryForLaw(
      PhraseEvolutionLawId::SparseDrift, RealizationLevel::P2Variation);
  const TrajectoryId sparseP3 = phraseTrajectoryForLaw(
      PhraseEvolutionLawId::SparseDrift, RealizationLevel::P3Transformation);
  std::printf("  I3 trajectory witness: DevelopReturn=%u/%u SparseDrift=%u/%u\n",
              static_cast<unsigned>(developP2),
              static_cast<unsigned>(developP3),
              static_cast<unsigned>(sparseP2),
              static_cast<unsigned>(sparseP3));
  expect("frozen I3 keeps P2/P3 level-dependent phrase trajectories",
         developP2 != developP3 && sparseP2 != sparseP3);
}

void testDepthDoesNotExpressRoleHierarchy() {
  struct FamilyCase {
    const char* name;
    GenerativeMode mode;
    uint8_t recipe;
  };
  constexpr FamilyCase families[] = {
      {"Lo-Fi", GenerativeMode::LoFi, kBaseRecipeId},
      {"Techno", GenerativeMode::Techno, kBaseRecipeId},
  };

  for (const FamilyCase& family : families) {
    const GenreSettings settings = settingsFor(family.mode, family.recipe);
    const SynthBObservation p1 = synthBObservation(
        settings, RealizationLevel::P1Canonical, 17);
    const SynthBObservation p2 = synthBObservation(
        settings, RealizationLevel::P2Variation, 17);
    const SynthBObservation p3 = synthBObservation(
        settings, RealizationLevel::P3Transformation, 17);

    printSynthB(family.name, "P1", p1);
    printSynthB(family.name, "P2", p2);
    printSynthB(family.name, "P3", p3);

    expect("P1/P2/P3 Synth-B materialization succeeds for shipped family",
           p1.ready && p2.ready && p3.ready);
    expect("profile-selected Synth-B role identity is unchanged by DEPTH",
           p1.selectedRole == p2.selectedRole &&
               p2.selectedRole == p3.selectedRole &&
               p1.materializedRole == p2.materializedRole &&
               p2.materializedRole == p3.materializedRole);
    expect("Synth-B role participation is unchanged by DEPTH",
           participationSignature(p1) == participationSignature(p2) &&
               participationSignature(p2) == participationSignature(p3));
    expect("Synth-B materialized activity is unchanged by DEPTH",
           sameMaterializedActivity(p1, p2) &&
               sameMaterializedActivity(p2, p3));
  }
}

void testSecondaryRoleIsASeparateOwnerAtSameDepth() {
  struct RoleCase {
    const char* name;
    GenerativeMode mode;
    CompositionSecondaryRole expectedProfileRole;
    SemanticSynthBRole expectedMaterialRole;
    uint8_t expectedParticipation;
  };
  constexpr RoleCase cases[] = {
      {"Reggae", GenerativeMode::Reggae, CompositionSecondaryRole::Chord,
       SemanticSynthBRole::Chord, 1u},
      {"Techno", GenerativeMode::Techno, CompositionSecondaryRole::Melodic,
       SemanticSynthBRole::Melodic, 2u},
      {"Lo-Fi", GenerativeMode::LoFi,
       CompositionSecondaryRole::ChordWithMelodicFill,
       SemanticSynthBRole::ChordWithMelodicFill, 3u},
  };

  for (const RoleCase& item : cases) {
    const GenreSettings settings = settingsFor(item.mode);
    const SynthBObservation observation = synthBObservation(
        settings, RealizationLevel::P2Variation, 17);
    printSynthB(item.name, "P2", observation);
    expect("same-depth secondary-role fixture materializes",
           observation.ready);
    expect("GenerationProfile secondaryRole is the selected semantic owner",
           observation.selectedRole == item.expectedProfileRole);
    expect("materialized Synth-B role follows profile.secondaryRole",
           observation.materializedRole == item.expectedMaterialRole &&
               observation.materializedRole ==
                   expectedSemanticRole(observation.selectedRole));
    expect("Synth-B participation branch follows secondaryRole at fixed DEPTH",
           participationSignature(observation) == item.expectedParticipation);
  }
}

}  // namespace

int main() {
  testCurrentDepthIsCausalRealizationMagnitude();
  testDepthDoesNotExpressRoleHierarchy();
  testSecondaryRoleIsASeparateOwnerAtSameDepth();

  if (g_failures != 0) {
    std::fprintf(stderr,
                 "GF2-I5 DEPTH / role-hierarchy characterization: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("GF2-I5 DEPTH / role-hierarchy characterization: PASS\n");
  return 0;
}
