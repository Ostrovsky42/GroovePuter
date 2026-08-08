#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct AtlasGrammarFixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[kRhythmRoleCount]{};
  ProtectedSpace protectedSpaces[2]{};
  LaneRelationship relationships[2]{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};
  uint8_t laneCount = 0;
  uint16_t aggregateMin = 0;
  uint16_t aggregateMax = 0;
};

void beginGrammar(AtlasGrammarFixture& fixture,
                  RhythmArchetypeId id,
                  RhythmFamily family) {
  fixture.trajectory.id = 1;
  fixture.trajectory.barCount = 1;
  fixture.trajectory.bars[0] = BarFunction::Statement;
  fixture.trajectoryRef.id = fixture.trajectory.id;
  fixture.trajectoryRef.weight = 100;
  fixture.trajectoryRef.allowedLevels = kAllRealizationLevels;

  fixture.archetype.id = id;
  fixture.archetype.family = family;
  fixture.archetype.allowedPhraseBars = phraseBarsBit(1);
  fixture.archetype.trajectories = &fixture.trajectoryRef;
  fixture.archetype.trajectoryCount = 1;

  fixture.catalog.archetypes = &fixture.archetype;
  fixture.catalog.archetypeCount = 1;
  fixture.catalog.trajectories = &fixture.trajectory;
  fixture.catalog.trajectoryCount = 1;
}

void addLane(AtlasGrammarFixture& fixture,
             RhythmRole role,
             StepMask canonical,
             StepMask preferred,
             StepMask optional,
             uint8_t structuralMin,
             uint8_t structuralMax,
             StepMask heldGate = 0) {
  assert(fixture.laneCount < kRhythmRoleCount);
  LaneGrammar& lane = fixture.lanes[fixture.laneCount++];
  lane.role = role;
  lane.canonicalAnchors = canonical;
  lane.preferred = preferred;
  lane.optional = optional;
  lane.heldGate = heldGate;
  lane.structuralMin = structuralMin;
  lane.structuralMax = structuralMax;
  lane.ornamentMax = 2;

  fixture.archetype.activeRoles = static_cast<RhythmRoleMask>(
      fixture.archetype.activeRoles | rhythmRoleBit(role));
  fixture.aggregateMin += structuralMin;
  fixture.aggregateMax += structuralMax;
}

void addProtectedSpace(AtlasGrammarFixture& fixture,
                       StepMask steps,
                       RhythmRoleMask roles) {
  assert(fixture.archetype.protectedSpaceCount < 2);
  ProtectedSpace& space =
      fixture.protectedSpaces[fixture.archetype.protectedSpaceCount++];
  space.steps = steps;
  space.affectedRoles = roles;
  fixture.archetype.protectedSpaces = fixture.protectedSpaces;
}

void addRelationship(AtlasGrammarFixture& fixture,
                     const LaneRelationship& relationship) {
  assert(fixture.archetype.relationshipCount < 2);
  fixture.relationships[fixture.archetype.relationshipCount++] = relationship;
  fixture.archetype.relationships = fixture.relationships;
}

void finishGrammar(AtlasGrammarFixture& fixture) {
  fixture.archetype.lanes = fixture.lanes;
  fixture.archetype.laneCount = fixture.laneCount;
  fixture.archetype.density = DensityContract{
      static_cast<uint8_t>(fixture.aggregateMin),
      static_cast<uint8_t>(fixture.aggregateMin),
      static_cast<uint8_t>(fixture.aggregateMax),
      8};
  fixture.archetype.mutation.level[0] = MutationBudget{};
  fixture.archetype.mutation.level[1] = MutationBudget{};
  fixture.archetype.mutation.level[2] = MutationBudget{};
  assert(validateRhythmCatalog(fixture.catalog));
}

void configureRollingAcidGrammar(AtlasGrammarFixture& fixture) {
  beginGrammar(fixture, 101, RhythmFamily::FourFloor);
  addLane(fixture, RhythmRole::Kick, 0x8888, 0, 0, 4, 4);
  addLane(fixture, RhythmRole::Backbeat, 0x0808, 0, 0, 2, 2);
  addLane(fixture, RhythmRole::ClosedHat, 0x2020, 0x0202, 0, 2, 4);
  addLane(fixture, RhythmRole::OpenHat, 0, 0x0202, 0x0101, 0, 2);
  addLane(fixture, RhythmRole::Percussion, 0, 0x1111, 0x4444, 1, 4);
  addLane(fixture, RhythmRole::BassRhythm, 0x8080, 0x7777, 0x0808, 4, 14);
  addLane(fixture, RhythmRole::MelodicRhythm, 0x2020, 0x0303, 0x0808, 2, 6);

  LaneRelationship relation{};
  relation.source = RhythmRole::Kick;
  relation.target = RhythmRole::ClosedHat;
  relation.op = RelationshipOp::Offset;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 2;
  addRelationship(fixture, relation);
  finishGrammar(fixture);
}

void configureClassicTwoStepGrammar(AtlasGrammarFixture& fixture) {
  beginGrammar(fixture, 102, RhythmFamily::UkTwoStep);
  addLane(fixture, RhythmRole::Kick, 0x8000, 0x0220, 0x0040, 2, 4);
  addLane(fixture, RhythmRole::Backbeat, 0x0808, 0, 0, 2, 2);
  addLane(fixture, RhythmRole::ClosedHat, 0x2020, 0x0505, 0x0202, 2, 6);
  addLane(fixture, RhythmRole::OpenHat, 0, 0x0101, 0x0202, 0, 2);
  addLane(fixture, RhythmRole::Percussion, 0x1000, 0x0456, 0x2020, 2, 6);
  addLane(fixture, RhythmRole::BassRhythm, 0x8000, 0x0442, 0x2020,
          2, 5, 0x8440);
  addLane(fixture, RhythmRole::MelodicRhythm, 0x2020, 0x1212, 0x0404, 2, 6);

  addProtectedSpace(
      fixture,
      0x0808,
      static_cast<RhythmRoleMask>(
          rhythmRoleBit(RhythmRole::Kick) |
          rhythmRoleBit(RhythmRole::BassRhythm)));

  LaneRelationship relation{};
  relation.source = RhythmRole::Backbeat;
  relation.target = RhythmRole::Kick;
  relation.op = RelationshipOp::Exclude;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  addRelationship(fixture, relation);
  finishGrammar(fixture);
}

void configureDeepChordGrammar(AtlasGrammarFixture& fixture) {
  beginGrammar(fixture, 103, RhythmFamily::DubPulse);
  addLane(fixture, RhythmRole::Kick, 0x8888, 0, 0, 4, 4);
  addLane(fixture, RhythmRole::Backbeat, 0x0808, 0, 0, 2, 2);
  addLane(fixture, RhythmRole::ClosedHat, 0x2020, 0x0202, 0x0101, 2, 5);
  addLane(fixture, RhythmRole::OpenHat, 0, 0x0202, 0x0101, 0, 2);
  addLane(fixture, RhythmRole::Percussion, 0, 0x1111, 0x2222, 1, 5);
  addLane(fixture, RhythmRole::BassRhythm, 0x8080, 0x1010, 0x0202,
          2, 4, 0x9090);
  addLane(fixture, RhythmRole::ChordRhythm, 0x2020, 0x0303, 0x0808, 2, 6);

  addProtectedSpace(
      fixture,
      0x4444,
      static_cast<RhythmRoleMask>(
          rhythmRoleBit(RhythmRole::Kick) |
          rhythmRoleBit(RhythmRole::BassRhythm) |
          rhythmRoleBit(RhythmRole::ChordRhythm)));

  LaneRelationship relation{};
  relation.source = RhythmRole::Kick;
  relation.target = RhythmRole::ChordRhythm;
  relation.op = RelationshipOp::Respond;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 3;
  relation.minResponsesPerWindow = 1;
  relation.maxResponsesPerWindow = 2;
  addRelationship(fixture, relation);
  finishGrammar(fixture);
}

uint64_t structuralSignature(const RhythmPhrasePlan& plan) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      hash ^= plan.bars[bar].roles[role].structural;
      hash *= 1099511628211ull;
      hash ^= plan.bars[bar].roles[role].secondary;
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

void assertRealizerAcceptsGrammar(const AtlasGrammarFixture& fixture,
                                  bool expectHeldGate) {
  uint64_t firstSignature = 0;
  bool sawFirst = false;
  bool sawDifferent = false;

  for (uint32_t seed = 0; seed < 128; ++seed) {
    RhythmRealizationRequest p1Request{};
    p1Request.catalog = &fixture.catalog;
    p1Request.archetypeId = fixture.archetype.id;
    p1Request.phraseBars = 1;
    p1Request.level = RealizationLevel::P1Canonical;
    p1Request.generation.projectSeed = seed;
    p1Request.generation.phraseOrdinal = 3;

    const RhythmRealizationResult p1 = realizeRhythmPhrase(p1Request);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);
    assert(p1.identity.trajectoryId == kNoTrajectoryId);
    assert(p1.plan.trajectoryId == kNoTrajectoryId);
    assert(p1.plan.intent == TransformationIntent::Auto);
    assert(p1.plan.bars[0].function == BarFunction::Statement);
    assert(planRespectsProtectedSpace(fixture.archetype, p1.plan));
    assert(planRespectsLaneBounds(fixture.archetype, p1.plan));
    assert(hardRelationshipsSatisfied(
        fixture.archetype, structuralOccupancy(p1.plan)));

    StepMask realizedHeld = 0;
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      realizedHeld = static_cast<StepMask>(
          realizedHeld | p1.plan.bars[0].roles[role].heldGate);
    }
    if (expectHeldGate) {
      assert(realizedHeld != 0);
    } else {
      assert(realizedHeld == 0);
    }

    const uint64_t signature = structuralSignature(p1.plan);
    if (!sawFirst) {
      firstSignature = signature;
      sawFirst = true;
    } else if (signature != firstSignature) {
      sawDifferent = true;
    }

    RhythmRealizationRequest p2Request = p1Request;
    p2Request.level = RealizationLevel::P2Variation;
    p2Request.reuseIdentity = &p1.identity;
    const RhythmRealizationResult p2 = realizeRhythmPhrase(p2Request);
    assert(p2.status != RealizationStatus::InvalidConstraintSet);
    assert(p2.identity.trajectoryId == kNoTrajectoryId);
    assert(p2.plan.bars[0].function == BarFunction::Statement);

    RhythmRealizationRequest p3Request = p2Request;
    p3Request.level = RealizationLevel::P3Transformation;
    const RhythmRealizationResult p3 = realizeRhythmPhrase(p3Request);
    assert(p3.status != RealizationStatus::InvalidConstraintSet);
    assert(p3.identity.trajectoryId == kNoTrajectoryId);
    assert(p3.plan.bars[0].function == BarFunction::Statement);
  }

  // The realizer must not collapse a generalized grammar back to one preset.
  assert(sawDifferent);
}

}  // namespace

int main() {
  AtlasGrammarFixture acid{};
  AtlasGrammarFixture ukg{};
  AtlasGrammarFixture dub{};
  configureRollingAcidGrammar(acid);
  configureClassicTwoStepGrammar(ukg);
  configureDeepChordGrammar(dub);

  assertRealizerAcceptsGrammar(acid, false);
  assertRealizerAcceptsGrammar(ukg, true);
  assertRealizerAcceptsGrammar(dub, true);
  return 0;
}
