#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "src/generation/rhythm/bar_evolution.h"

using namespace GroovePuterRhythm;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

uint8_t bitCount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

uint16_t ghostCount(const RhythmBarPlan& bar) {
  uint16_t count = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    count += bitCount16(bar.roles[role].ghosts);
  }
  return count;
}

constexpr LaneGrammar makeLane() {
  LaneGrammar lane{};
  lane.role = RhythmRole::Kick;
  lane.canonicalAnchors = stepBit(0);
  lane.optional = static_cast<StepMask>(
      stepBit(12) | stepBit(13) | stepBit(14) | stepBit(15));
  lane.structuralMin = 1;
  lane.structuralMax = 1;
  lane.ornamentMax = 4;
  return lane;
}

constexpr LaneGrammar kLane[] = {makeLane()};

constexpr BarTrajectory kBudgetTrajectories[] = {
    {1, 2,
     {BarFunction::Statement, BarFunction::RepeatWithGhosts,
      BarFunction::Statement, BarFunction::Statement}},
    {2, 2,
     {BarFunction::Statement, BarFunction::Turnaround,
      BarFunction::Statement, BarFunction::Statement}},
};

constexpr TrajectoryRef kBudgetTrajectoryRefs[] = {
    {1, 100, kAllRealizationLevels},
    {2, 100, realizationLevelBit(RealizationLevel::P3Transformation)},
};

RhythmArchetype budgetArchetype(const MutationBudget& p3,
                                const TrajectoryRef* refs =
                                    kBudgetTrajectoryRefs,
                                uint8_t refCount = 2) {
  RhythmArchetype archetype{};
  archetype.id = 910;
  archetype.family = RhythmFamily::MachineSyncopation;
  archetype.allowedPhraseBars = phraseBarsBit(2);
  archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
  archetype.lanes = kLane;
  archetype.laneCount = 1;
  archetype.trajectories = refs;
  archetype.trajectoryCount = refCount;
  archetype.density = DensityContract{1, 1, 1, 4};
  archetype.mutation.level[
      static_cast<uint8_t>(RealizationLevel::P3Transformation)] = p3;
  return archetype;
}

BarEvolutionRequest makeRequest(const RhythmCatalogView& catalog,
                                RhythmArchetypeId archetypeId,
                                uint8_t bars,
                                RealizationLevel level,
                                TrajectoryId trajectory) {
  BarEvolutionRequest request{};
  request.catalog = &catalog;
  request.archetypeId = archetypeId;
  request.phraseBars = bars;
  request.level = level;
  request.generation.projectSeed = 0xBADC0DEu;
  request.generation.phraseOrdinal = 7;
  request.requestedTrajectoryId = trajectory;
  return request;
}

void requireOk(const BarEvolutionResult& result, const char* message) {
  require(result.status == BarEvolutionStatus::Ok, message);
  require(result.realizationStatus == RealizationStatus::Ok ||
              result.realizationStatus == RealizationStatus::ValidButSparse,
          "base realization did not succeed");
}

void testZeroAddBudgetIsAuthoritative() {
  MutationBudget p3{};
  p3.maxAdds = 0;
  p3.flags = AllowOptionalAdds;

  RhythmArchetype archetype = budgetArchetype(
      p3, kBudgetTrajectoryRefs, 1);
  RhythmCatalogView catalog{
      &archetype,
      1,
      kBudgetTrajectories,
      static_cast<uint8_t>(sizeof(kBudgetTrajectories) /
                           sizeof(kBudgetTrajectories[0]))};
  require(static_cast<bool>(validateRhythmCatalog(catalog)),
          "zero-add RepeatWithGhosts catalog must validate");

  const BarEvolutionResult repeated = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 2,
                  RealizationLevel::P3Transformation, 1));
  requireOk(repeated, "RepeatWithGhosts budget regression failed");
  require(ghostCount(repeated.plan.bars[1]) ==
              ghostCount(repeated.plan.bars[0]),
          "RepeatWithGhosts added ornaments with maxAdds == 0");
}

void testAddFlagsAreAuthoritative() {
  MutationBudget p3{};
  p3.maxAdds = 2;
  p3.flags = AllowTurnaround;
  p3.allowedIntents =
      transformationIntentBit(TransformationIntent::Turnaround);

  RhythmArchetype archetype = budgetArchetype(p3);
  RhythmCatalogView catalog{
      &archetype,
      1,
      kBudgetTrajectories,
      static_cast<uint8_t>(sizeof(kBudgetTrajectories) /
                           sizeof(kBudgetTrajectories[0]))};
  require(static_cast<bool>(validateRhythmCatalog(catalog)),
          "no-add-flag Stage 6 catalog must validate");

  const BarEvolutionResult repeated = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 2,
                  RealizationLevel::P3Transformation, 1));
  requireOk(repeated, "RepeatWithGhosts add-flag regression failed");
  require(ghostCount(repeated.plan.bars[1]) ==
              ghostCount(repeated.plan.bars[0]),
          "RepeatWithGhosts ignored missing add-capable flags");

  const BarEvolutionResult turnaround = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 2,
                  RealizationLevel::P3Transformation, 2));
  requireOk(turnaround, "Turnaround add-flag regression failed");
  require(ghostCount(turnaround.plan.bars[1]) == 0,
          "Turnaround ignored missing add-capable flags");
}

constexpr LaneGrammar transactionLane(RhythmRole role,
                                      uint8_t anchorStep) {
  LaneGrammar lane{};
  lane.role = role;
  lane.canonicalAnchors = stepBit(anchorStep);
  lane.optional = stepBit(4);
  lane.structuralMin = 1;
  lane.structuralMax = 2;
  lane.ornamentMax = 0;
  return lane;
}

constexpr LaneGrammar kTransactionLanes[] = {
    transactionLane(RhythmRole::Kick, 0),
    transactionLane(RhythmRole::BassRhythm, 1),
};

constexpr LaneRelationship kTransactionRelationships[] = {
    {RhythmRole::Kick,
     RhythmRole::BassRhythm,
     RelationshipOp::Coincide,
     ConstraintStrength::Hard,
     RelationshipScope::Phrase,
     kAllSteps,
     0,
     0,
     1,
     1,
     0,
     0,
     0},
};

constexpr BarTrajectory kRuntimeInvalidTrajectories[] = {
    {3, 2,
     {BarFunction::Statement, BarFunction::Repeat,
      BarFunction::Statement, BarFunction::Statement}},
};

constexpr TrajectoryRef kRuntimeInvalidRefs[] = {
    {3, 100, kAllRealizationLevels},
};

RhythmArchetype runtimeInvalidArchetype() {
  RhythmArchetype archetype{};
  archetype.id = 911;
  archetype.family = RhythmFamily::MachineSyncopation;
  archetype.allowedPhraseBars = phraseBarsBit(2);
  archetype.activeRoles = static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::Kick) |
      rhythmRoleBit(RhythmRole::BassRhythm));
  archetype.lanes = kTransactionLanes;
  archetype.laneCount = 2;
  archetype.relationships = kTransactionRelationships;
  archetype.relationshipCount = 1;
  archetype.trajectories = kRuntimeInvalidRefs;
  archetype.trajectoryCount = 1;
  archetype.density = DensityContract{2, 2, 4, 0};
  return archetype;
}

void testEvolutionFailureIsTransactional() {
  RhythmArchetype archetype = runtimeInvalidArchetype();
  RhythmCatalogView catalog{
      &archetype,
      1,
      kRuntimeInvalidTrajectories,
      1};
  const CatalogValidationResult validation = validateRhythmCatalog(catalog);
  if (!validation) {
    std::fprintf(stderr,
                 "transaction fixture validation error=%u archetype=%u item=%u\n",
                 static_cast<unsigned>(validation.error),
                 static_cast<unsigned>(validation.archetypeIndex),
                 static_cast<unsigned>(validation.itemIndex));
  }
  require(static_cast<bool>(validation),
          "runtime-only EvolutionInvalid fixture catalog must validate");

  const BarEvolutionResult result = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 2,
                  RealizationLevel::P1Canonical, 3));
  require(result.realizationStatus == RealizationStatus::Ok ||
              result.realizationStatus == RealizationStatus::ValidButSparse,
          "transaction test did not reach evolution after base realization");
  require(result.status == BarEvolutionStatus::EvolutionInvalid,
          "phrase-scope relationship violation did not fail evolution");
  require(result.trajectoryId == kNoTrajectoryId,
          "failed evolution leaked selected trajectory metadata");
  require(result.plan.barCount == 0,
          "failed evolution leaked a partially prepared plan");
  require(result.identity.archetypeId == kNoArchetypeId,
          "failed evolution leaked a partially prepared identity");
}

}  // namespace

int main() {
  testZeroAddBudgetIsAuthoritative();
  testAddFlagsAreAuthoritative();
  testEvolutionFailureIsTransactional();
  std::puts("Groove Vocabulary Stage 6 contract regressions: OK");
  return 0;
}
