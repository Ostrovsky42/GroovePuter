#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct RespondChoiceFixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[2]{};
  LaneRelationship relationship{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  RespondChoiceFixture() {
    trajectory.id = 41;
    trajectory.barCount = 1;
    trajectory.bars[0] = BarFunction::Statement;

    lanes[0].role = RhythmRole::Kick;
    lanes[0].immutableAnchors =
        static_cast<StepMask>(stepBit(4) | stepBit(8));
    lanes[0].structuralMin = 2;
    lanes[0].structuralMax = 2;

    lanes[1].role = RhythmRole::BassRhythm;
    lanes[1].canonicalAnchors = stepBit(5);
    lanes[1].optional =
        static_cast<StepMask>(stepBit(6) | stepBit(9));
    lanes[1].structuralMin = 1;
    lanes[1].structuralMax = 2;

    relationship.source = RhythmRole::Kick;
    relationship.target = RhythmRole::BassRhythm;
    relationship.op = RelationshipOp::Respond;
    relationship.strength = ConstraintStrength::Hard;
    relationship.scope = RelationshipScope::BarLocal;
    relationship.zoneMask = kAllSteps;
    relationship.minOffset = 1;
    relationship.maxOffset = 2;
    relationship.minResponsesPerWindow = 1;
    relationship.maxResponsesPerWindow = 0;

    trajectoryRef.id = trajectory.id;
    trajectoryRef.weight = 100;
    trajectoryRef.allowedLevels = kAllRealizationLevels;

    archetype.id = 206;
    archetype.family = RhythmFamily::MachineSyncopation;
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
    archetype.density = DensityContract{3, 4, 4, 0};
    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{};
    archetype.mutation.level[2] = MutationBudget{};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

struct RespondVariationFixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[2]{};
  LaneRelationship relationship{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  RespondVariationFixture() {
    trajectory.id = 42;
    trajectory.barCount = 1;
    trajectory.bars[0] = BarFunction::Statement;

    // P1 is allowed to have no Kick source at all. P2 may consider the
    // optional step 8 source, but doing so opens a mandatory +1 Respond
    // window at target step 9.
    lanes[0].role = RhythmRole::Kick;
    lanes[0].optional = stepBit(8);
    lanes[0].structuralMin = 0;
    lanes[0].structuralMax = 1;

    // Bass is already full at structuralMax == 1 and its canonical onset is
    // deliberately outside the response window of the optional Kick source.
    lanes[1].role = RhythmRole::BassRhythm;
    lanes[1].canonicalAnchors = stepBit(5);
    lanes[1].structuralMin = 1;
    lanes[1].structuralMax = 1;

    relationship.source = RhythmRole::Kick;
    relationship.target = RhythmRole::BassRhythm;
    relationship.op = RelationshipOp::Respond;
    relationship.strength = ConstraintStrength::Hard;
    relationship.scope = RelationshipScope::BarLocal;
    relationship.zoneMask = kAllSteps;
    relationship.minOffset = 1;
    relationship.maxOffset = 1;
    relationship.minResponsesPerWindow = 1;
    relationship.maxResponsesPerWindow = 1;

    trajectoryRef.id = trajectory.id;
    trajectoryRef.weight = 100;
    trajectoryRef.allowedLevels = kAllRealizationLevels;

    archetype.id = 207;
    archetype.family = RhythmFamily::MachineSyncopation;
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
    archetype.density = DensityContract{1, 1, 2, 0};
    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{
        1, 0, 0, 0,
        AllowOptionalAdds,
        0};
    archetype.mutation.level[2] = MutationBudget{};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

void testRespondRepairChoosesProgressMakingTarget() {
  RespondChoiceFixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint32_t seed = 0; seed < 256; ++seed) {
    RhythmRealizationRequest request{};
    request.catalog = &fixture.catalog;
    request.archetypeId = fixture.archetype.id;
    request.phraseBars = 1;
    request.level = RealizationLevel::P1Canonical;
    request.generation.projectSeed = seed;
    request.generation.phraseOrdinal = 13;

    const RhythmRealizationResult result = realizeRhythmPhrase(request);
    assert(result.status != RealizationStatus::InvalidConstraintSet);
    assert(result.plan.trajectoryId == kNoTrajectoryId);
    assert(result.plan.bars[0].function == BarFunction::Statement);

    const PhraseOccupancy occupancy = structuralOccupancy(result.plan);
    assert(hardRelationshipsSatisfied(fixture.archetype, occupancy));

    const StepMask kick =
        occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)];
    const StepMask bass =
        occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)];

    assert(kick == static_cast<StepMask>(stepBit(4) | stepBit(8)));
    // Step 5 is the canonical response to source step 4. The only remaining
    // structural slot must serve source step 8; choosing step 6 merely adds a
    // second response to the already-satisfied source and makes the feasible
    // solution unreachable under structuralMax == 2.
    assert(bass == static_cast<StepMask>(stepBit(5) | stepBit(9)));
  }
}

void testOptionalRespondSourceCannotInvalidateLegalIdentity() {
  RespondVariationFixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint32_t seed = 0; seed < 256; ++seed) {
    RhythmRealizationRequest p1Request{};
    p1Request.catalog = &fixture.catalog;
    p1Request.archetypeId = fixture.archetype.id;
    p1Request.phraseBars = 1;
    p1Request.level = RealizationLevel::P1Canonical;
    p1Request.generation.projectSeed = seed;
    p1Request.generation.phraseOrdinal = 17;

    const RhythmRealizationResult p1 = realizeRhythmPhrase(p1Request);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);
    assert(hardRelationshipsSatisfied(
        fixture.archetype, structuralOccupancy(p1.plan)));

    RhythmRealizationRequest p2Request = p1Request;
    p2Request.level = RealizationLevel::P2Variation;
    p2Request.reuseIdentity = &p1.identity;

    const RhythmRealizationResult p2 = realizeRhythmPhrase(p2Request);
    // The optional Kick source at step 8 would require a Bass response at
    // step 9, but Bass is already at structuralMax. Variation must skip that
    // optional candidate rather than turn a legal shared identity invalid.
    assert(p2.status != RealizationStatus::InvalidConstraintSet);
    const PhraseOccupancy occupancy = structuralOccupancy(p2.plan);
    assert(hardRelationshipsSatisfied(fixture.archetype, occupancy));
    assert(occupancy.roleMasks[0]
               [static_cast<uint8_t>(RhythmRole::Kick)] == 0);
    assert(occupancy.roleMasks[0]
               [static_cast<uint8_t>(RhythmRole::BassRhythm)] == stepBit(5));
  }
}

}  // namespace

int main() {
  testRespondRepairChoosesProgressMakingTarget();
  testOptionalRespondSourceCannotInvalidateLegalIdentity();
  return 0;
}
