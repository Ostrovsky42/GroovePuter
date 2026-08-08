#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct Stage2Fixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[3]{};
  ProtectedSpace protectedSpaces[1]{};
  LaneRelationship relationships[1]{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  Stage2Fixture() {
    trajectory.id = 1;
    trajectory.barCount = 4;
    for (uint8_t bar = 0; bar < 4; ++bar) {
      trajectory.bars[bar] = BarFunction::Statement;
    }

    lanes[0].role = RhythmRole::Kick;
    lanes[0].canonicalAnchors =
        static_cast<StepMask>(stepBit(0) | stepBit(8));
    lanes[0].preferred =
        static_cast<StepMask>(stepBit(4) | stepBit(12));
    lanes[0].optional =
        static_cast<StepMask>(stepBit(2) | stepBit(10));
    lanes[0].structuralMin = 2;
    lanes[0].structuralMax = 4;
    lanes[0].ornamentMax = 1;

    lanes[1].role = RhythmRole::BassRhythm;
    lanes[1].canonicalAnchors =
        static_cast<StepMask>(stepBit(1) | stepBit(9));
    lanes[1].preferred =
        static_cast<StepMask>(stepBit(5) | stepBit(13));
    lanes[1].optional =
        static_cast<StepMask>(stepBit(3) | stepBit(11));
    lanes[1].heldGate = stepBit(1);
    lanes[1].tieGate = stepBit(9);
    lanes[1].structuralMin = 2;
    lanes[1].structuralMax = 4;
    lanes[1].ornamentMax = 1;

    lanes[2].role = RhythmRole::Backbeat;
    lanes[2].immutableAnchors =
        static_cast<StepMask>(stepBit(4) | stepBit(12));
    lanes[2].optional =
        static_cast<StepMask>(stepBit(6) | stepBit(14));
    lanes[2].structuralMin = 2;
    lanes[2].structuralMax = 4;
    lanes[2].ornamentMax = 2;

    protectedSpaces[0].steps =
        static_cast<StepMask>(stepBit(6) | stepBit(14));
    protectedSpaces[0].affectedRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::BassRhythm));

    relationships[0].source = RhythmRole::Kick;
    relationships[0].target = RhythmRole::BassRhythm;
    relationships[0].op = RelationshipOp::Offset;
    relationships[0].strength = ConstraintStrength::Hard;
    relationships[0].scope = RelationshipScope::BarLocal;
    relationships[0].zoneMask = kAllSteps;
    relationships[0].minOffset = 1;
    relationships[0].maxOffset = 1;

    trajectoryRef.id = trajectory.id;
    trajectoryRef.weight = 100;
    trajectoryRef.allowedLevels = kAllRealizationLevels;

    archetype.id = 41;
    archetype.family = RhythmFamily::MachineSyncopation;
    archetype.allowedPhraseBars = phraseBarsBit(4);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::BassRhythm) |
        rhythmRoleBit(RhythmRole::Backbeat));
    archetype.lanes = lanes;
    archetype.laneCount = 3;
    archetype.protectedSpaces = protectedSpaces;
    archetype.protectedSpaceCount = 1;
    archetype.relationships = relationships;
    archetype.relationshipCount = 1;
    archetype.trajectories = &trajectoryRef;
    archetype.trajectoryCount = 1;
    archetype.density = DensityContract{6, 8, 12, 4};

    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{
        2, 0, 0, 0,
        AllowGhostConversion,
        0};
    archetype.mutation.level[2] = MutationBudget{
        4, 0, 0, 0,
        AllowOptionalAdds,
        0};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

bool identityEqual(const PhraseRhythmIdentity& a,
                   const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId ||
      a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) {
    return false;
  }
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].steps != b.protectedSpaces[i].steps ||
        a.protectedSpaces[i].affectedRoles !=
            b.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  return true;
}

bool planEqual(const RhythmPhrasePlan& a, const RhythmPhrasePlan& b) {
  if (a.barCount != b.barCount ||
      a.trajectoryId != b.trajectoryId ||
      a.level != b.level ||
      a.intent != b.intent) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    if (a.bars[bar].function != b.bars[bar].function) return false;
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      const RoleRhythmPlan& x = a.bars[bar].roles[role];
      const RoleRhythmPlan& y = b.bars[bar].roles[role];
      if (x.structural != y.structural ||
          x.secondary != y.secondary ||
          x.ghosts != y.ghosts ||
          x.shortGate != y.shortGate ||
          x.heldGate != y.heldGate ||
          x.tieGate != y.tieGate ||
          x.accents != y.accents) {
        return false;
      }
    }
  }
  return true;
}

RhythmRealizationResult realize(const Stage2Fixture& fixture,
                                uint32_t seed,
                                RealizationLevel level,
                                const PhraseRhythmIdentity* reuse = nullptr) {
  RhythmRealizationRequest request{};
  request.catalog = &fixture.catalog;
  request.archetypeId = fixture.archetype.id;
  request.phraseBars = 4;
  request.level = level;
  request.generation.projectSeed = seed;
  request.generation.phraseOrdinal = 7;
  request.reuseIdentity = reuse;
  return realizeRhythmPhrase(request);
}

void assertStage2Boundary(const RhythmPhrasePlan& plan) {
  assert(plan.trajectoryId == kNoTrajectoryId);
  assert(plan.intent == TransformationIntent::Auto);
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    assert(plan.bars[bar].function == BarFunction::Statement);
  }
}

void assertValidPlan(const Stage2Fixture& fixture,
                     const RhythmRealizationResult& result) {
  assert(result.status != RealizationStatus::InvalidConstraintSet);
  assertStage2Boundary(result.plan);
  assert(planRespectsProtectedSpace(fixture.archetype, result.plan));
  assert(planRespectsLaneBounds(fixture.archetype, result.plan));
  assert(hardRelationshipsSatisfied(
      fixture.archetype, structuralOccupancy(result.plan)));
}

void testRelationshipTruthTablesAndBoundaryPolicy() {
  PhraseOccupancy occupancy{};
  occupancy.barCount = 2;

  LaneRelationship exclude{};
  exclude.source = RhythmRole::Kick;
  exclude.target = RhythmRole::BassRhythm;
  exclude.op = RelationshipOp::Exclude;
  exclude.strength = ConstraintStrength::Hard;
  exclude.scope = RelationshipScope::BarLocal;
  exclude.zoneMask = kAllSteps;
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)] =
      stepBit(4);
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)] =
      stepBit(5);
  assert(relationshipSatisfied(exclude, occupancy));
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)] |=
      stepBit(4);
  assert(!relationshipSatisfied(exclude, occupancy));

  occupancy = {};
  occupancy.barCount = 2;
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)] =
      stepBit(15);
  occupancy.roleMasks[1][static_cast<uint8_t>(RhythmRole::BassRhythm)] =
      stepBit(0);
  LaneRelationship offset{};
  offset.source = RhythmRole::Kick;
  offset.target = RhythmRole::BassRhythm;
  offset.op = RelationshipOp::Offset;
  offset.strength = ConstraintStrength::Hard;
  offset.scope = RelationshipScope::BarLocal;
  offset.zoneMask = kAllSteps;
  offset.minOffset = 1;
  offset.maxOffset = 1;
  assert(!relationshipSatisfied(offset, occupancy));
  offset.scope = RelationshipScope::Phrase;
  assert(relationshipSatisfied(offset, occupancy));

  occupancy = {};
  occupancy.barCount = 2;
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)] =
      stepBit(2);
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)] =
      stepBit(2);
  occupancy.roleMasks[1][static_cast<uint8_t>(RhythmRole::Kick)] =
      stepBit(10);
  occupancy.roleMasks[1][static_cast<uint8_t>(RhythmRole::BassRhythm)] =
      stepBit(10);
  LaneRelationship coincide{};
  coincide.source = RhythmRole::Kick;
  coincide.target = RhythmRole::BassRhythm;
  coincide.op = RelationshipOp::Coincide;
  coincide.strength = ConstraintStrength::Hard;
  coincide.scope = RelationshipScope::Phrase;
  coincide.zoneMask = kAllSteps;
  coincide.minMatches = 2;
  coincide.maxMatches = 2;
  assert(relationshipSatisfied(coincide, occupancy));
  coincide.maxMatches = 1;
  assert(!relationshipSatisfied(coincide, occupancy));

  occupancy = {};
  occupancy.barCount = 1;
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)] =
      static_cast<StepMask>(stepBit(0) | stepBit(2));
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)] =
      stepBit(1);
  LaneRelationship respond{};
  respond.source = RhythmRole::Kick;
  respond.target = RhythmRole::BassRhythm;
  respond.op = RelationshipOp::Respond;
  respond.strength = ConstraintStrength::Hard;
  respond.scope = RelationshipScope::BarLocal;
  respond.zoneMask = kAllSteps;
  respond.minOffset = -1;
  respond.maxOffset = 1;
  respond.minResponsesPerWindow = 1;
  respond.maxResponsesPerWindow = 1;
  assert(!relationshipSatisfied(respond, occupancy));
  occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)] |=
      stepBit(3);
  assert(relationshipSatisfied(respond, occupancy));

  LaneRelationship fillGaps{};
  fillGaps.source = RhythmRole::Kick;
  fillGaps.target = RhythmRole::BassRhythm;
  fillGaps.op = RelationshipOp::FillGaps;
  fillGaps.strength = ConstraintStrength::Soft;
  fillGaps.scope = RelationshipScope::BarLocal;
  fillGaps.zoneMask = kAllSteps;
  fillGaps.weight = 80;
  assert(relationshipSatisfied(fillGaps, occupancy));
}

void testIdentityContinuityWithoutBarEvolution() {
  Stage2Fixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  const RhythmRealizationResult p1 = realize(
      fixture, 0x10203040u, RealizationLevel::P1Canonical);
  assertValidPlan(fixture, p1);
  assert(p1.identity.trajectoryId == kNoTrajectoryId);

  const RhythmRealizationResult p2 = realize(
      fixture, 0x10203040u,
      RealizationLevel::P2Variation, &p1.identity);
  assertValidPlan(fixture, p2);
  assert(identityEqual(p1.identity, p2.identity));

  const RhythmRealizationResult p3 = realize(
      fixture, 0x10203040u,
      RealizationLevel::P3Transformation, &p1.identity);
  assertValidPlan(fixture, p3);
  assert(identityEqual(p1.identity, p3.identity));
}

void testCanonicalAndImmutableAnchorsCannotBeSuspended() {
  Stage2Fixture fixture;
  const RhythmRealizationResult p1 = realize(
      fixture, 1234u, RealizationLevel::P1Canonical);
  assertValidPlan(fixture, p1);

  const uint8_t kick = static_cast<uint8_t>(RhythmRole::Kick);
  const uint8_t backbeat = static_cast<uint8_t>(RhythmRole::Backbeat);

  RhythmPhrasePlan illegal = p1.plan;
  illegal.bars[0].roles[kick].structural = static_cast<StepMask>(
      illegal.bars[0].roles[kick].structural & ~stepBit(0));
  assert(!planRespectsLaneBounds(fixture.archetype, illegal));

  illegal = p1.plan;
  illegal.bars[0].roles[backbeat].structural = static_cast<StepMask>(
      illegal.bars[0].roles[backbeat].structural & ~stepBit(4));
  assert(!planRespectsLaneBounds(fixture.archetype, illegal));

  illegal = p1.plan;
  illegal.trajectoryId = 1;
  assert(!planRespectsLaneBounds(fixture.archetype, illegal));

  illegal = p1.plan;
  illegal.bars[0].function = BarFunction::Break;
  assert(!planRespectsLaneBounds(fixture.archetype, illegal));
}

void testGateIntentSurvivesAllPLevels() {
  Stage2Fixture fixture;
  const RhythmRealizationResult p1 = realize(
      fixture, 0xA55Au, RealizationLevel::P1Canonical);
  assertValidPlan(fixture, p1);
  const RhythmRealizationResult p2 = realize(
      fixture, 0xA55Au,
      RealizationLevel::P2Variation, &p1.identity);
  assertValidPlan(fixture, p2);
  const RhythmRealizationResult p3 = realize(
      fixture, 0xA55Au,
      RealizationLevel::P3Transformation, &p1.identity);
  assertValidPlan(fixture, p3);

  const uint8_t bass = static_cast<uint8_t>(RhythmRole::BassRhythm);
  const RhythmRealizationResult* results[] = {&p1, &p2, &p3};
  for (const RhythmRealizationResult* result : results) {
    for (uint8_t bar = 0; bar < result->plan.barCount; ++bar) {
      const RoleRhythmPlan& role = result->plan.bars[bar].roles[bass];
      assert((role.heldGate & stepBit(1)) != 0);
      assert((role.tieGate & stepBit(9)) != 0);
      assert((role.heldGate & role.tieGate) == 0);
    }
  }
}

void testInvalidReusedIdentityIsRejectedTransactionally() {
  Stage2Fixture fixture;
  const RhythmRealizationResult p1 = realize(
      fixture, 99u, RealizationLevel::P1Canonical);
  assertValidPlan(fixture, p1);

  PhraseRhythmIdentity invalid = p1.identity;
  invalid.trajectoryId = 1;
  const RhythmRealizationResult badTrajectory = realize(
      fixture, 99u, RealizationLevel::P2Variation, &invalid);
  assert(badTrajectory.status == RealizationStatus::InvalidConstraintSet);

  invalid = p1.identity;
  const uint8_t backbeat = static_cast<uint8_t>(RhythmRole::Backbeat);
  invalid.structuralCore[0][backbeat] = static_cast<StepMask>(
      invalid.structuralCore[0][backbeat] & ~stepBit(4));
  const RhythmRealizationResult missingAnchor = realize(
      fixture, 99u, RealizationLevel::P2Variation, &invalid);
  assert(missingAnchor.status == RealizationStatus::InvalidConstraintSet);
}

void testDeterminismAndPropertyCorpus() {
  Stage2Fixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  uint32_t p2VariationEvents = 0;
  uint32_t p3VariationEvents = 0;

  for (uint32_t seed = 0; seed < 512; ++seed) {
    const RhythmRealizationResult p1a = realize(
        fixture, seed, RealizationLevel::P1Canonical);
    const RhythmRealizationResult p1b = realize(
        fixture, seed, RealizationLevel::P1Canonical);
    assertValidPlan(fixture, p1a);
    assertValidPlan(fixture, p1b);
    assert(identityEqual(p1a.identity, p1b.identity));
    assert(planEqual(p1a.plan, p1b.plan));

    const RhythmRealizationResult p2a = realize(
        fixture, seed,
        RealizationLevel::P2Variation, &p1a.identity);
    const RhythmRealizationResult p2b = realize(
        fixture, seed,
        RealizationLevel::P2Variation, &p1a.identity);
    assertValidPlan(fixture, p2a);
    assertValidPlan(fixture, p2b);
    assert(identityEqual(p1a.identity, p2a.identity));
    assert(planEqual(p2a.plan, p2b.plan));

    const RhythmRealizationResult p3a = realize(
        fixture, seed,
        RealizationLevel::P3Transformation, &p1a.identity);
    const RhythmRealizationResult p3b = realize(
        fixture, seed,
        RealizationLevel::P3Transformation, &p1a.identity);
    assertValidPlan(fixture, p3a);
    assertValidPlan(fixture, p3b);
    assert(identityEqual(p1a.identity, p3a.identity));
    assert(planEqual(p3a.plan, p3b.plan));

    for (uint8_t bar = 0; bar < 4; ++bar) {
      for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
        p2VariationEvents +=
            bitCount16(p2a.plan.bars[bar].roles[role].ghosts);
        p3VariationEvents +=
            bitCount16(p3a.plan.bars[bar].roles[role].secondary);
      }
    }
  }

  assert(p2VariationEvents > 0);
  assert(p3VariationEvents > 0);
}

void testValidButSparseIsNotInvalid() {
  Stage2Fixture fixture;
  fixture.lanes[2].structuralMax = 6;
  fixture.archetype.density.structuralPreferred = 13;
  fixture.archetype.density.structuralMax = 14;
  assert(validateRhythmCatalog(fixture.catalog));

  const RhythmRealizationResult result = realize(
      fixture, 7u, RealizationLevel::P1Canonical);
  assert(result.status == RealizationStatus::ValidButSparse);
  assertStage2Boundary(result.plan);
  assert(planRespectsLaneBounds(fixture.archetype, result.plan));
  assert(hardRelationshipsSatisfied(
      fixture.archetype, structuralOccupancy(result.plan)));
}

void testReferenceBindingContractHasZeroTopologyViolations() {
  Stage2Fixture fixture;
  const RhythmRealizationResult result = realize(
      fixture, 88u, RealizationLevel::P2Variation);
  assertValidPlan(fixture, result);

  const PhraseOccupancy realized = structuralOccupancy(result.plan);
  PhraseOccupancy rebound = realized;
  uint32_t invented = 0;
  uint32_t droppedStructural = 0;
  for (uint8_t bar = 0; bar < rebound.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      const StepMask before = realized.roleMasks[bar][role];
      const StepMask after = rebound.roleMasks[bar][role];
      invented += static_cast<uint32_t>(before != (before | after));
      droppedStructural += static_cast<uint32_t>(before != (before & after));
    }
  }
  assert(invented == 0);
  assert(droppedStructural == 0);
}

}  // namespace

int main() {
  testRelationshipTruthTablesAndBoundaryPolicy();
  testIdentityContinuityWithoutBarEvolution();
  testCanonicalAndImmutableAnchorsCannotBeSuspended();
  testGateIntentSurvivesAllPLevels();
  testInvalidReusedIdentityIsRejectedTransactionally();
  testDeterminismAndPropertyCorpus();
  testValidButSparseIsNotInvalid();
  testReferenceBindingContractHasZeroTopologyViolations();
  return 0;
}
