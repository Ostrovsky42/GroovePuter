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

constexpr LaneGrammar makeLane() {
  LaneGrammar lane{};
  lane.role = RhythmRole::Kick;
  lane.canonicalAnchors = stepBit(0);
  lane.preferred = stepBit(4);
  lane.optional = static_cast<StepMask>(
      stepBit(8) | stepBit(12));
  lane.structuralMin = 2;
  lane.structuralMax = 3;
  lane.ornamentMax = 0;
  return lane;
}

constexpr LaneGrammar kLanes[] = {makeLane()};

constexpr BarTrajectory kTrajectories[] = {
    {1, 2,
     {BarFunction::Statement, BarFunction::Response,
      BarFunction::Statement, BarFunction::Statement}},
    {2, 2,
     {BarFunction::Statement, BarFunction::Reduction,
      BarFunction::Statement, BarFunction::Statement}},
    {3, 2,
     {BarFunction::Statement, BarFunction::Break,
      BarFunction::Statement, BarFunction::Statement}},
};

constexpr TrajectoryRef kTrajectoryRefs[] = {
    {1, 100, kAllRealizationLevels},
    {2, 100, realizationLevelBit(RealizationLevel::P3Transformation)},
    {3, 100, realizationLevelBit(RealizationLevel::P3Transformation)},
};

constexpr RhythmArchetype makeArchetype() {
  RhythmArchetype archetype{};
  archetype.id = 920;
  archetype.family = RhythmFamily::MachineSyncopation;
  archetype.allowedPhraseBars = phraseBarsBit(2);
  archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
  archetype.lanes = kLanes;
  archetype.laneCount = 1;
  archetype.trajectories = kTrajectoryRefs;
  archetype.trajectoryCount = 3;
  archetype.density = DensityContract{2, 2, 3, 0};

  MutationBudget p3{};
  p3.maxAdds = 2;
  p3.maxDrops = 1;
  p3.flags = static_cast<uint16_t>(
      AllowOptionalAdds | AllowPreferredDrops |
      AllowReduction | AllowBreak);
  p3.allowedIntents = static_cast<TransformationIntentMask>(
      transformationIntentBit(TransformationIntent::Reduce) |
      transformationIntentBit(TransformationIntent::Break));
  archetype.mutation.level[
      static_cast<uint8_t>(RealizationLevel::P3Transformation)] = p3;
  return archetype;
}

constexpr RhythmArchetype kArchetypes[] = {makeArchetype()};
constexpr RhythmCatalogView kCatalog = {
    kArchetypes,
    1,
    kTrajectories,
    static_cast<uint8_t>(sizeof(kTrajectories) / sizeof(kTrajectories[0]))};

BarEvolutionRequest request(TrajectoryId trajectory, uint16_t ordinal = 7) {
  BarEvolutionRequest value{};
  value.catalog = &kCatalog;
  value.archetypeId = kArchetypes[0].id;
  value.phraseBars = 2;
  value.level = RealizationLevel::P3Transformation;
  value.generation.projectSeed = 0x61A7E001u;
  value.generation.phraseOrdinal = ordinal;
  value.requestedTrajectoryId = trajectory;
  return value;
}

RhythmRealizationResult baseRealization(const BarEvolutionRequest& evolution) {
  RhythmRealizationRequest base{};
  base.catalog = evolution.catalog;
  base.archetypeId = evolution.archetypeId;
  base.phraseBars = evolution.phraseBars;
  base.level = evolution.level;
  base.generation = evolution.generation;
  return realizeRhythmPhrase(base);
}

bool equalRole(const RoleRhythmPlan& a, const RoleRhythmPlan& b) {
  return a.structural == b.structural &&
         a.secondary == b.secondary &&
         a.ghosts == b.ghosts &&
         a.shortGate == b.shortGate &&
         a.heldGate == b.heldGate &&
         a.tieGate == b.tieGate &&
         a.accents == b.accents;
}

bool equalBarEvents(const RhythmBarPlan& a, const RhythmBarPlan& b) {
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!equalRole(a.roles[role], b.roles[role])) return false;
  }
  return true;
}

uint16_t structuralCount(const RhythmBarPlan& bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(static_cast<StepMask>(
        bar.roles[role].structural | bar.roles[role].secondary));
  }
  return total;
}

uint16_t secondaryCount(const RhythmBarPlan& bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(bar.roles[role].secondary);
  }
  return total;
}

void requireBaseOk(const RhythmRealizationResult& result) {
  require(result.status == RealizationStatus::Ok ||
              result.status == RealizationStatus::ValidButSparse,
          "Stage 6.1 base realization failed");
}

void requireEvolutionOk(const BarEvolutionResult& result) {
  require(result.status == BarEvolutionStatus::Ok,
          "Stage 6.1 evolution failed");
  require(result.realizationStatus == RealizationStatus::Ok ||
              result.realizationStatus == RealizationStatus::ValidButSparse,
          "Stage 6.1 evolution lost base status");
  require(evolvedPlanValid(kArchetypes[0], result.plan),
          "Stage 6.1 evolution returned invalid plan");
}

void testStatementAndResponseAreBaseTopology() {
  const BarEvolutionRequest evolution = request(1);
  const RhythmRealizationResult base = baseRealization(evolution);
  requireBaseOk(base);
  const BarEvolutionResult evolved = evolveRhythmPhrase(evolution);
  requireEvolutionOk(evolved);

  require(evolved.plan.bars[0].function == BarFunction::Statement,
          "Statement metadata missing");
  require(evolved.plan.bars[1].function == BarFunction::Response,
          "Response metadata missing");
  require(equalBarEvents(evolved.plan.bars[0], base.plan.bars[0]),
          "Statement changed independently realized base events");
  require(equalBarEvents(evolved.plan.bars[1], base.plan.bars[1]),
          "Response v1 must remain metadata-only");
}

void requireOneSecondaryDrop(TrajectoryId trajectory,
                             BarFunction expectedFunction,
                             const char* noSecondaryMessage,
                             const char* noDropMessage) {
  const BarEvolutionRequest evolution = request(trajectory);
  const RhythmRealizationResult base = baseRealization(evolution);
  requireBaseOk(base);
  require(secondaryCount(base.plan.bars[1]) >= 1,
          noSecondaryMessage);

  const BarEvolutionResult evolved = evolveRhythmPhrase(evolution);
  requireEvolutionOk(evolved);
  require(evolved.plan.bars[1].function == expectedFunction,
          "Stage 6.1 drop function metadata missing");

  const uint16_t beforeTotal = structuralCount(base.plan.bars[1]);
  const uint16_t afterTotal = structuralCount(evolved.plan.bars[1]);
  const uint16_t beforeSecondary = secondaryCount(base.plan.bars[1]);
  const uint16_t afterSecondary = secondaryCount(evolved.plan.bars[1]);

  require(beforeTotal == afterTotal + 1,
          noDropMessage);
  require(beforeSecondary == afterSecondary + 1,
          "drop did not consume a secondary event first");
  require(evolved.plan.bars[1].roles[
              static_cast<uint8_t>(RhythmRole::Kick)].structural ==
              base.plan.bars[1].roles[
              static_cast<uint8_t>(RhythmRole::Kick)].structural,
          "drop consumed structural core while a secondary candidate existed");

  const MutationBudget& budget = kArchetypes[0].mutation.level[
      static_cast<uint8_t>(RealizationLevel::P3Transformation)];
  require(beforeTotal - afterTotal <= budget.maxDrops,
          "drop exceeded maxDrops budget");
}

void testReductionActuallyDropsWithinBudget() {
  requireOneSecondaryDrop(
      2, BarFunction::Reduction,
      "Reduction fixture did not create a removable secondary event",
      "Reduction did not perform its bounded drop");
}

void testBreakActuallyDropsWithinBudget() {
  requireOneSecondaryDrop(
      3, BarFunction::Break,
      "Break fixture did not create a removable secondary event",
      "Break did not perform its bounded drop");
}

void testMalformedCatalogFailsBeforeLookup() {
  RhythmCatalogView malformed{};
  malformed.archetypeCount = 1;
  malformed.trajectoryCount = 1;

  BarEvolutionRequest evolution = request(1);
  evolution.catalog = &malformed;
  const BarEvolutionResult result = evolveRhythmPhrase(evolution);

  require(result.status == BarEvolutionStatus::BaseRealizationFailed,
          "malformed catalog must fail through Stage 2 validation");
  require(result.trajectoryId == kNoTrajectoryId,
          "malformed catalog exposed trajectory state");
  require(result.plan.barCount == 0,
          "malformed catalog exposed a partial plan");
}

void testFixedCapacityFootprintGuard() {
  constexpr uint16_t kDropCandidateUpperBound =
      kRhythmRoleCount * kStepsPerBar * 2u;
  require(kDropCandidateUpperBound <= 256,
          "drop candidate upper bound grew beyond reviewed Stage 6.1 cost");
  require(sizeof(RhythmPhrasePlan) <= 512,
          "RhythmPhrasePlan exceeded reviewed Stage 6.1 stack footprint");
  require(sizeof(BarEvolutionResult) <= 704,
          "BarEvolutionResult exceeded reviewed Stage 6.1 stack footprint");
}

}  // namespace

int main() {
  require(static_cast<bool>(validateRhythmCatalog(kCatalog)),
          "Stage 6.1 fixture catalog must validate");
  testStatementAndResponseAreBaseTopology();
  testReductionActuallyDropsWithinBudget();
  testBreakActuallyDropsWithinBudget();
  testMalformedCatalogFailsBeforeLookup();
  testFixedCapacityFootprintGuard();
  std::puts("Groove Vocabulary Stage 6.1 hardening: OK");
  return 0;
}
