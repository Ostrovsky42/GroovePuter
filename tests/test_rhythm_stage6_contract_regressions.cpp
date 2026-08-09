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

RhythmArchetype budgetArchetype(const MutationBudget& p3) {
  RhythmArchetype archetype{};
  archetype.id = 910;
  archetype.family = RhythmFamily::MachineSyncopation;
  archetype.allowedPhraseBars = phraseBarsBit(2);
  archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
  archetype.lanes = kLane;
  archetype.laneCount = 1;
  archetype.trajectories = kBudgetTrajectoryRefs;
  archetype.trajectoryCount = 2;
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

void exerciseNoGhostAdds(const MutationBudget& budget,
                         const char* catalogMessage) {
  RhythmArchetype archetype = budgetArchetype(budget);
  RhythmCatalogView catalog{
      &archetype,
      1,
      kBudgetTrajectories,
      static_cast<uint8_t>(sizeof(kBudgetTrajectories) /
                           sizeof(kBudgetTrajectories[0]))};
  require(static_cast<bool>(validateRhythmCatalog(catalog)), catalogMessage);

  const BarEvolutionResult repeated = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 2,
                  RealizationLevel::P3Transformation, 1));
  requireOk(repeated, "RepeatWithGhosts budget regression failed");
  require(ghostCount(repeated.plan.bars[1]) ==
              ghostCount(repeated.plan.bars[0]),
          "RepeatWithGhosts added ornaments outside the add budget/flags");

  const BarEvolutionResult turnaround = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 2,
                  RealizationLevel::P3Transformation, 2));
  requireOk(turnaround, "Turnaround budget regression failed");
  require(ghostCount(turnaround.plan.bars[1]) == 0,
          "Turnaround added ornaments outside the add budget/flags");
}

void testZeroAddBudgetIsAuthoritative() {
  MutationBudget p3{};
  p3.maxAdds = 0;
  p3.flags = static_cast<uint16_t>(AllowOptionalAdds | AllowTurnaround);
  p3.allowedIntents =
      transformationIntentBit(TransformationIntent::Turnaround);
  exerciseNoGhostAdds(p3, "zero-add Stage 6 catalog must validate");
}

void testAddFlagsAreAuthoritative() {
  MutationBudget p3{};
  p3.maxAdds = 2;
  p3.flags = AllowTurnaround;
  p3.allowedIntents =
      transformationIntentBit(TransformationIntent::Turnaround);
  exerciseNoGhostAdds(p3, "no-add-flag Stage 6 catalog must validate");
}

constexpr BarTrajectory kInvalidEvolutionTrajectories[] = {
    {3, 1,
     {BarFunction::Repeat, BarFunction::Statement,
      BarFunction::Statement, BarFunction::Statement}},
};

constexpr TrajectoryRef kInvalidEvolutionRefs[] = {
    {3, 100, kAllRealizationLevels},
};

RhythmArchetype invalidEvolutionArchetype() {
  RhythmArchetype archetype{};
  archetype.id = 911;
  archetype.family = RhythmFamily::MachineSyncopation;
  archetype.allowedPhraseBars = phraseBarsBit(1);
  archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
  archetype.lanes = kLane;
  archetype.laneCount = 1;
  archetype.trajectories = kInvalidEvolutionRefs;
  archetype.trajectoryCount = 1;
  archetype.density = DensityContract{1, 1, 1, 4};
  return archetype;
}

void testEvolutionFailureIsTransactional() {
  RhythmArchetype archetype = invalidEvolutionArchetype();
  RhythmCatalogView catalog{
      &archetype,
      1,
      kInvalidEvolutionTrajectories,
      1};
  require(static_cast<bool>(validateRhythmCatalog(catalog)),
          "EvolutionInvalid fixture catalog must validate");

  const BarEvolutionResult result = evolveRhythmPhrase(
      makeRequest(catalog, archetype.id, 1,
                  RealizationLevel::P1Canonical, 3));
  require(result.realizationStatus == RealizationStatus::Ok ||
              result.realizationStatus == RealizationStatus::ValidButSparse,
          "transaction test did not reach evolution after base realization");
  require(result.status == BarEvolutionStatus::EvolutionInvalid,
          "invalid bar function position did not fail evolution");
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
