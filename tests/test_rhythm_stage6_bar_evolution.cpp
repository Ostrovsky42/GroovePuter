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

constexpr LaneGrammar lane(RhythmRole role,
                           StepMask immutableAnchors,
                           StepMask canonicalAnchors,
                           StepMask preferred,
                           StepMask optional,
                           uint8_t structuralMin,
                           uint8_t structuralMax,
                           uint8_t ornamentMax) {
  LaneGrammar value{};
  value.role = role;
  value.immutableAnchors = immutableAnchors;
  value.canonicalAnchors = canonicalAnchors;
  value.preferred = preferred;
  value.optional = optional;
  value.structuralMin = structuralMin;
  value.structuralMax = structuralMax;
  value.ornamentMax = ornamentMax;
  return value;
}

constexpr LaneRelationship hardExclude(RhythmRole source,
                                        RhythmRole target) {
  LaneRelationship value{};
  value.source = source;
  value.target = target;
  value.op = RelationshipOp::Exclude;
  value.strength = ConstraintStrength::Hard;
  value.scope = RelationshipScope::BarLocal;
  value.zoneMask = kAllSteps;
  return value;
}

constexpr MutationPolicy mutationPolicy() {
  MutationPolicy policy{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P1Canonical)] =
      MutationBudget{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)] =
      MutationBudget{
          2,
          1,
          0,
          0,
          static_cast<uint16_t>(AllowGhostConversion |
                                AllowPreferredDrops |
                                AllowReduction),
          transformationIntentBit(TransformationIntent::Reduce)};
  policy.level[static_cast<uint8_t>(RealizationLevel::P3Transformation)] =
      MutationBudget{
          3,
          3,
          0,
          0,
          static_cast<uint16_t>(AllowOptionalAdds |
                                AllowPreferredDrops |
                                AllowReduction |
                                AllowTurnaround |
                                AllowBreak),
          static_cast<TransformationIntentMask>(
              transformationIntentBit(TransformationIntent::Reduce) |
              transformationIntentBit(TransformationIntent::Turnaround) |
              transformationIntentBit(TransformationIntent::Break))};
  return policy;
}

constexpr LaneGrammar kLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0),
         stepBit(8),
         stepBit(6) | stepBit(14),
         stepBit(3) | stepBit(11),
         2, 4, 2),
    lane(RhythmRole::Backbeat,
         0,
         stepBit(4) | stepBit(12),
         0,
         0,
         2, 2, 1),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         stepBit(1) | stepBit(5) | stepBit(9) | stepBit(13),
         stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
         4, 6, 4),
};

constexpr LaneRelationship kRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

constexpr BarTrajectory kTrajectories[] = {
    {1, 1,
     {BarFunction::Statement, BarFunction::Statement,
      BarFunction::Statement, BarFunction::Statement}},
    {2, 2,
     {BarFunction::Statement, BarFunction::Repeat,
      BarFunction::Statement, BarFunction::Statement}},
    {3, 2,
     {BarFunction::Statement, BarFunction::RepeatWithGhosts,
      BarFunction::Statement, BarFunction::Statement}},
    {4, 3,
     {BarFunction::Statement, BarFunction::Response,
      BarFunction::Return, BarFunction::Statement}},
    {5, 4,
     {BarFunction::Statement, BarFunction::Response,
      BarFunction::Repeat, BarFunction::Return}},
    {6, 4,
     {BarFunction::Statement, BarFunction::Repeat,
      BarFunction::Reduction, BarFunction::Return}},
    {7, 4,
     {BarFunction::Statement, BarFunction::Build,
      BarFunction::RepeatWithGhosts, BarFunction::Turnaround}},
    {8, 4,
     {BarFunction::Statement, BarFunction::Response,
      BarFunction::Break, BarFunction::Return}},
};

constexpr TrajectoryRef kTrajectoryRefs[] = {
    {1, 100, kAllRealizationLevels},
    {2, 70, kAllRealizationLevels},
    {3, 30,
     static_cast<RealizationLevelMask>(
         realizationLevelBit(RealizationLevel::P2Variation) |
         realizationLevelBit(RealizationLevel::P3Transformation))},
    {4, 100, kAllRealizationLevels},
    {5, 70, kAllRealizationLevels},
    {6, 30,
     static_cast<RealizationLevelMask>(
         realizationLevelBit(RealizationLevel::P2Variation) |
         realizationLevelBit(RealizationLevel::P3Transformation))},
    {7, 20, realizationLevelBit(RealizationLevel::P3Transformation)},
    {8, 20, realizationLevelBit(RealizationLevel::P3Transformation)},
};

constexpr RhythmArchetype makeArchetype() {
  RhythmArchetype value{};
  value.id = 900;
  value.family = RhythmFamily::MachineSyncopation;
  value.allowedPhraseBars = kAllPhraseBars;
  value.activeRoles = static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::Kick) |
      rhythmRoleBit(RhythmRole::Backbeat) |
      rhythmRoleBit(RhythmRole::ClosedHat));
  value.lanes = kLanes;
  value.laneCount = 3;
  value.relationships = kRelationships;
  value.relationshipCount = 1;
  value.trajectories = kTrajectoryRefs;
  value.trajectoryCount = 8;
  value.density = DensityContract{8, 10, 12, 6};
  value.mutation = mutationPolicy();
  return value;
}

constexpr RhythmArchetype kArchetypes[] = {makeArchetype()};
constexpr RhythmCatalogView kCatalog = {
    kArchetypes,
    1,
    kTrajectories,
    static_cast<uint8_t>(sizeof(kTrajectories) / sizeof(kTrajectories[0]))};

BarEvolutionRequest request(uint8_t bars,
                            RealizationLevel level,
                            TrajectoryId trajectory,
                            uint16_t ordinal = 17) {
  BarEvolutionRequest value{};
  value.catalog = &kCatalog;
  value.archetypeId = 900;
  value.phraseBars = bars;
  value.level = level;
  value.generation.projectSeed = 0x1234ABCDu;
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
  base.reuseIdentity = evolution.reuseIdentity;
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

bool equalPlan(const RhythmPhrasePlan& a, const RhythmPhrasePlan& b) {
  if (a.barCount != b.barCount ||
      a.trajectoryId != b.trajectoryId ||
      a.level != b.level ||
      a.intent != b.intent) {
    return false;
  }
  for (uint8_t bar = 0; bar < a.barCount; ++bar) {
    if (a.bars[bar].function != b.bars[bar].function ||
        !equalBarEvents(a.bars[bar], b.bars[bar])) {
      return false;
    }
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

uint16_t ghostCount(const RhythmBarPlan& bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(bar.roles[role].ghosts);
  }
  return total;
}

bool hasLateGhost(const RhythmBarPlan& bar) {
  const StepMask late = static_cast<StepMask>(
      stepBit(12) | stepBit(13) | stepBit(14) | stepBit(15));
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (bar.roles[role].ghosts & late) return true;
  }
  return false;
}

void requireOk(const BarEvolutionResult& result, const char* message) {
  require(result.status == BarEvolutionStatus::Ok, message);
  require(result.realizationStatus == RealizationStatus::Ok ||
              result.realizationStatus == RealizationStatus::ValidButSparse,
          "BarEvolution lost base realization status");
  require(result.plan.barCount >= 1 && result.plan.barCount <= kMaxPhraseBars,
          "BarEvolution returned invalid bar count");
  require(evolvedPlanValid(kArchetypes[0], result.plan),
          "BarEvolution returned an invalid evolved plan");
}

void testCatalogAndDeterminism() {
  require(static_cast<bool>(validateRhythmCatalog(kCatalog)),
          "Stage 6 fixture catalog must validate");

  const BarEvolutionRequest selected =
      request(4, RealizationLevel::P2Variation, kNoTrajectoryId, 9);
  const BarEvolutionResult first = evolveRhythmPhrase(selected);
  const BarEvolutionResult second = evolveRhythmPhrase(selected);
  requireOk(first, "deterministic Stage 6 selection failed");
  requireOk(second, "repeat deterministic Stage 6 selection failed");
  require(first.trajectoryId == second.trajectoryId &&
              equalPlan(first.plan, second.plan),
          "same Stage 6 context changed trajectory or plan");

  bool sawFive = false;
  bool sawSix = false;
  for (uint16_t ordinal = 0; ordinal < 96; ++ordinal) {
    const BarEvolutionResult varied = evolveRhythmPhrase(
        request(4, RealizationLevel::P2Variation,
                kNoTrajectoryId, ordinal));
    requireOk(varied, "weighted Stage 6 selection failed");
    sawFive |= varied.trajectoryId == 5;
    sawSix |= varied.trajectoryId == 6;
  }
  require(sawFive && sawSix,
          "pattern/phrase ordinal did not exercise multiple legal trajectories");
}

void testRepeatAndReturn() {
  const BarEvolutionResult repeat = evolveRhythmPhrase(
      request(2, RealizationLevel::P1Canonical, 2));
  requireOk(repeat, "Repeat trajectory failed");
  require(repeat.plan.bars[1].function == BarFunction::Repeat,
          "Repeat function metadata missing");
  require(equalBarEvents(repeat.plan.bars[0], repeat.plan.bars[1]),
          "Repeat did not preserve previous bar events");

  const BarEvolutionResult responseReturn = evolveRhythmPhrase(
      request(3, RealizationLevel::P2Variation, 4));
  requireOk(responseReturn, "Response/Return trajectory failed");
  require(responseReturn.plan.bars[1].function == BarFunction::Response,
          "Response function metadata missing");
  require(responseReturn.plan.bars[2].function == BarFunction::Return,
          "Return function metadata missing");
  require(equalBarEvents(responseReturn.plan.bars[0],
                         responseReturn.plan.bars[2]),
          "Return did not restore statement bar events");
}

void testRepeatWithGhostsAndBuild() {
  const BarEvolutionResult ghostRepeat = evolveRhythmPhrase(
      request(2, RealizationLevel::P3Transformation, 3));
  requireOk(ghostRepeat, "RepeatWithGhosts trajectory failed");
  require(ghostRepeat.plan.bars[1].function == BarFunction::RepeatWithGhosts,
          "RepeatWithGhosts metadata missing");
  require(structuralCount(ghostRepeat.plan.bars[0]) ==
              structuralCount(ghostRepeat.plan.bars[1]),
          "RepeatWithGhosts changed structural density");
  require(ghostCount(ghostRepeat.plan.bars[1]) >
              ghostCount(ghostRepeat.plan.bars[0]),
          "RepeatWithGhosts did not add legal ornamentation");

  const BarEvolutionRequest buildRequest =
      request(4, RealizationLevel::P3Transformation, 7);
  const RhythmRealizationResult base = baseRealization(buildRequest);
  require(base.status == RealizationStatus::Ok ||
              base.status == RealizationStatus::ValidButSparse,
          "Build base realization failed");
  const BarEvolutionResult build = evolveRhythmPhrase(buildRequest);
  requireOk(build, "Build/Turnaround trajectory failed");
  require(build.plan.bars[1].function == BarFunction::Build,
          "Build metadata missing");
  require(ghostCount(build.plan.bars[1]) > ghostCount(base.plan.bars[1]),
          "Build did not increase legal ornament density");
  require(build.plan.bars[3].function == BarFunction::Turnaround,
          "Turnaround metadata missing");
  require(hasLateGhost(build.plan.bars[3]),
          "Turnaround did not create a late-bar cue");
}

void testReductionAndBreak() {
  const BarEvolutionRequest reductionRequest =
      request(4, RealizationLevel::P2Variation, 6);
  const RhythmRealizationResult reductionBase =
      baseRealization(reductionRequest);
  require(reductionBase.status == RealizationStatus::Ok ||
              reductionBase.status == RealizationStatus::ValidButSparse,
          "Reduction base realization failed");
  const BarEvolutionResult reduction = evolveRhythmPhrase(reductionRequest);
  requireOk(reduction, "Reduction trajectory failed");
  require(reduction.plan.bars[2].function == BarFunction::Reduction,
          "Reduction metadata missing");
  require(ghostCount(reduction.plan.bars[2]) == 0,
          "Reduction retained ghost ornamentation");
  require(structuralCount(reduction.plan.bars[2]) <=
              structuralCount(reductionBase.plan.bars[2]),
          "Reduction increased structural density");

  const BarEvolutionRequest breakRequest =
      request(4, RealizationLevel::P3Transformation, 8);
  const RhythmRealizationResult breakBase = baseRealization(breakRequest);
  require(breakBase.status == RealizationStatus::Ok ||
              breakBase.status == RealizationStatus::ValidButSparse,
          "Break base realization failed");
  const BarEvolutionResult broken = evolveRhythmPhrase(breakRequest);
  requireOk(broken, "Break trajectory failed");
  require(broken.plan.bars[2].function == BarFunction::Break,
          "Break metadata missing");
  require(ghostCount(broken.plan.bars[2]) == 0,
          "Break retained ghost ornamentation");
  require(structuralCount(broken.plan.bars[2]) <=
              structuralCount(breakBase.plan.bars[2]),
          "Break increased structural density");
}

void testInvalidRequestsAndIdentityReuse() {
  BarEvolutionRequest invalid =
      request(0, RealizationLevel::P1Canonical, 1);
  require(evolveRhythmPhrase(invalid).status ==
              BarEvolutionStatus::InvalidRequest,
          "zero-bar request was accepted");

  invalid = request(4, RealizationLevel::P1Canonical, 8);
  require(evolveRhythmPhrase(invalid).status ==
              BarEvolutionStatus::NoEligibleTrajectory,
          "P1 accepted a P3 Break trajectory");

  const BarEvolutionResult first = evolveRhythmPhrase(
      request(4, RealizationLevel::P3Transformation, 7, 33));
  requireOk(first, "identity source evolution failed");
  BarEvolutionRequest reused =
      request(4, RealizationLevel::P3Transformation, 7, 34);
  reused.reuseIdentity = &first.identity;
  const BarEvolutionResult second = evolveRhythmPhrase(reused);
  requireOk(second, "identity reuse evolution failed");
  require(second.identity.archetypeId == first.identity.archetypeId &&
              second.identity.phraseBars == first.identity.phraseBars,
          "identity reuse changed phrase identity metadata");
  for (uint8_t bar = 0; bar < first.identity.phraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      require(second.identity.structuralCore[bar][role] ==
                  first.identity.structuralCore[bar][role] &&
                  second.identity.canonicalCore[bar][role] ==
                  first.identity.canonicalCore[bar][role],
              "BarEvolution mutated reused phrase identity");
    }
  }
}

}  // namespace

int main() {
  testCatalogAndDeterminism();
  testRepeatAndReturn();
  testRepeatWithGhostsAndBuild();
  testReductionAndBreak();
  testInvalidRequestsAndIdentityReuse();
  std::puts("Groove Vocabulary Stage 6 BarEvolution: OK");
  return 0;
}
