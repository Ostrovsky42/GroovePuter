#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/generation/rhythm/rhythm_catalog.h"

using namespace GroovePuterRhythm;

namespace {

struct Fixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[2]{};
  ProtectedSpace protectedSpace{};
  LaneRelationship relationship{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  Fixture() {
    trajectory.id = 1;
    trajectory.barCount = 1;
    trajectory.bars[0] = BarFunction::Statement;

    lanes[0].role = RhythmRole::Kick;
    lanes[0].canonicalAnchors = stepBit(0);
    lanes[0].preferred = stepBit(4);
    lanes[0].optional = stepBit(8);
    lanes[0].structuralMin = 1;
    lanes[0].structuralMax = 3;

    lanes[1].role = RhythmRole::BassRhythm;
    lanes[1].canonicalAnchors = stepBit(1);
    lanes[1].preferred = stepBit(5);
    lanes[1].optional = stepBit(9);
    lanes[1].structuralMin = 1;
    lanes[1].structuralMax = 3;

    relationship.source = RhythmRole::Kick;
    relationship.target = RhythmRole::BassRhythm;
    relationship.op = RelationshipOp::Offset;
    relationship.strength = ConstraintStrength::Hard;
    relationship.scope = RelationshipScope::BarLocal;
    relationship.zoneMask = kAllSteps;
    relationship.minOffset = 1;
    relationship.maxOffset = 1;

    trajectoryRef.id = trajectory.id;
    trajectoryRef.weight = 100;
    trajectoryRef.allowedLevels = kAllRealizationLevels;

    archetype.id = 1;
    archetype.family = RhythmFamily::FourFloor;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::BassRhythm));
    archetype.lanes = lanes;
    archetype.laneCount = 2;
    archetype.relationships = &relationship;
    archetype.relationshipCount = 1;
    archetype.trajectories = &trajectoryRef;
    archetype.trajectoryCount = 1;
    archetype.density = DensityContract{2, 3, 6, 0};
    archetype.mutation.level[0] = MutationBudget{0, 0, 0, 0, 0, 0};
    archetype.mutation.level[1] = MutationBudget{0, 0, 0, 0, 0, 0};
    archetype.mutation.level[2] = MutationBudget{0, 0, 0, 0, 0, 0};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

void expectError(const RhythmCatalogView& catalog,
                 CatalogValidationError expected) {
  const CatalogValidationResult result = validateRhythmCatalog(catalog);
  assert(!result);
  assert(result.error == expected);
}

void testValidBoundedCatalog() {
  Fixture f;
  assert(validateRhythmCatalog(f.catalog));
  static_assert(kStepsPerBar == 16, "Core v1 grid must stay at 16 steps");
  static_assert(kMaxPhraseBars == 4, "Core v1 phrase bound changed");
  static_assert(sizeof(PhraseRhythmIdentity) <= 256,
                "PhraseRhythmIdentity exceeded Stage 1 budget");
}

void testLaneAndProtectedSpaceContradictions() {
  Fixture overlap;
  overlap.lanes[0].forbidden = overlap.lanes[0].canonicalAnchors;
  expectError(overlap.catalog, CatalogValidationError::OverlappingLaneZones);

  Fixture protectedAnchor;
  protectedAnchor.protectedSpace.steps = stepBit(0);
  protectedAnchor.protectedSpace.affectedRoles =
      rhythmRoleBit(RhythmRole::Kick);
  protectedAnchor.archetype.protectedSpaces = &protectedAnchor.protectedSpace;
  protectedAnchor.archetype.protectedSpaceCount = 1;
  expectError(protectedAnchor.catalog,
              CatalogValidationError::ProtectedSpaceAnchorConflict);

  Fixture overlappingGate;
  overlappingGate.lanes[0].shortGate = stepBit(0);
  overlappingGate.lanes[0].heldGate = stepBit(0);
  expectError(overlappingGate.catalog,
              CatalogValidationError::InvalidLaneGateMasks);

  Fixture gateOutsideOnsetSpace;
  gateOutsideOnsetSpace.lanes[0].heldGate = stepBit(15);
  expectError(gateOutsideOnsetSpace.catalog,
              CatalogValidationError::InvalidLaneGateMasks);
}

void testHardRelationshipFeasibilityAndNoWrap() {
  Fixture unreachable;
  unreachable.relationship.minOffset = 2;
  unreachable.relationship.maxOffset = 2;
  expectError(unreachable.catalog,
              CatalogValidationError::ImpossibleHardRelationship);

  Fixture noWrap;
  noWrap.lanes[0].canonicalAnchors = stepBit(15);
  noWrap.lanes[0].preferred = 0;
  noWrap.lanes[0].optional = 0;
  noWrap.lanes[1].canonicalAnchors = stepBit(0);
  noWrap.lanes[1].preferred = 0;
  noWrap.lanes[1].optional = 0;
  noWrap.relationship.scope = RelationshipScope::BarLocal;
  expectError(noWrap.catalog,
              CatalogValidationError::ImpossibleHardRelationship);
}

void testRespondRequiresDistinctOwnedTargets() {
  Fixture f;
  f.lanes[0].canonicalAnchors =
      static_cast<StepMask>(stepBit(0) | stepBit(2));
  f.lanes[0].preferred = 0;
  f.lanes[0].optional = 0;
  f.lanes[0].structuralMin = 2;
  f.lanes[0].structuralMax = 2;
  f.lanes[1].canonicalAnchors = 0;
  f.lanes[1].preferred = stepBit(1);
  f.lanes[1].optional = 0;
  f.lanes[1].structuralMin = 1;
  f.lanes[1].structuralMax = 2;
  f.relationship.op = RelationshipOp::Respond;
  f.relationship.minOffset = -1;
  f.relationship.maxOffset = 1;
  f.relationship.minResponsesPerWindow = 1;
  f.relationship.maxResponsesPerWindow = 1;
  f.archetype.density = DensityContract{3, 3, 4, 0};
  expectError(f.catalog, CatalogValidationError::ImpossibleHardRelationship);
}

void testPLevelTrajectoryPolicyComposition() {
  Fixture p1Break;
  p1Break.trajectory.bars[0] = BarFunction::Break;
  p1Break.trajectoryRef.allowedLevels =
      realizationLevelBit(RealizationLevel::P1Canonical);
  expectError(p1Break.catalog, CatalogValidationError::TrajectoryLevelConflict);

  Fixture p2Reduction;
  p2Reduction.trajectory.bars[0] = BarFunction::Reduction;
  p2Reduction.trajectoryRef.allowedLevels =
      realizationLevelBit(RealizationLevel::P2Variation);
  expectError(p2Reduction.catalog,
              CatalogValidationError::TrajectoryLevelConflict);
}

void testPhraseCoordinateCoincideCardinality() {
  Fixture f;
  f.trajectory.barCount = 4;
  f.trajectory.bars[0] = BarFunction::Statement;
  f.trajectory.bars[1] = BarFunction::Repeat;
  f.trajectory.bars[2] = BarFunction::Repeat;
  f.trajectory.bars[3] = BarFunction::Repeat;
  f.archetype.allowedPhraseBars = phraseBarsBit(4);
  f.lanes[0].canonicalAnchors = stepBit(0);
  f.lanes[0].preferred = 0;
  f.lanes[0].optional = 0;
  f.lanes[0].structuralMin = 1;
  f.lanes[0].structuralMax = 1;
  f.lanes[1].canonicalAnchors = stepBit(0);
  f.lanes[1].preferred = 0;
  f.lanes[1].optional = 0;
  f.lanes[1].structuralMin = 1;
  f.lanes[1].structuralMax = 1;
  f.relationship.op = RelationshipOp::Coincide;
  f.relationship.scope = RelationshipScope::Phrase;
  f.relationship.minOffset = 0;
  f.relationship.maxOffset = 0;
  f.relationship.minMatches = 4;
  f.relationship.maxMatches = 4;
  f.archetype.density = DensityContract{2, 2, 2, 0};
  assert(validateRhythmCatalog(f.catalog));

  f.relationship.minMatches = 1;
  f.relationship.maxMatches = 3;
  expectError(f.catalog, CatalogValidationError::ImpossibleHardRelationship);
}

void testCoincideCannotExceedRemainingLaneCapacity() {
  Fixture f;
  f.lanes[0].canonicalAnchors = stepBit(0);
  f.lanes[0].preferred = stepBit(4);
  f.lanes[0].optional = 0;
  f.lanes[0].structuralMin = 1;
  f.lanes[0].structuralMax = 1;
  f.lanes[1].canonicalAnchors = stepBit(1);
  f.lanes[1].preferred = stepBit(4);
  f.lanes[1].optional = 0;
  f.lanes[1].structuralMin = 1;
  f.lanes[1].structuralMax = 1;
  f.relationship.op = RelationshipOp::Coincide;
  f.relationship.scope = RelationshipScope::BarLocal;
  f.relationship.minOffset = 0;
  f.relationship.maxOffset = 0;
  f.relationship.minMatches = 1;
  f.relationship.maxMatches = 0;
  f.archetype.density = DensityContract{2, 2, 2, 0};
  expectError(f.catalog, CatalogValidationError::ImpossibleHardRelationship);
}

void testStage1SemanticTypesRemainDistinct() {
  static_assert(static_cast<uint8_t>(RealizationStatus::ValidButSparse) !=
                    static_cast<uint8_t>(RealizationStatus::InvalidConstraintSet),
                "Sparse and invalid states must not alias");
  RhythmEventIntent event{};
  event.step = 3;
  event.gate = GateClass::Tie;
  event.importance = EventImportance::Ghost;
  assert(event.gate == GateClass::Tie);
  assert(event.importance == EventImportance::Ghost);
}

}  // namespace

int main() {
  testValidBoundedCatalog();
  testLaneAndProtectedSpaceContradictions();
  testHardRelationshipFeasibilityAndNoWrap();
  testRespondRequiresDistinctOwnedTargets();
  testPLevelTrajectoryPolicyComposition();
  testPhraseCoordinateCoincideCardinality();
  testCoincideCannotExceedRemainingLaneCapacity();
  testStage1SemanticTypesRemainDistinct();
  return 0;
}
