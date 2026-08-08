#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct CoincideChoiceFixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[3]{};
  LaneRelationship relationships[2]{};
  TrajectoryRef ref{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  CoincideChoiceFixture() {
    trajectory.id = 31;
    trajectory.barCount = 1;
    trajectory.bars[0] = BarFunction::Statement;

    lanes[0].role = RhythmRole::Kick;
    lanes[0].optional = static_cast<StepMask>(stepBit(4) | stepBit(8));
    lanes[0].structuralMin = 0;
    lanes[0].structuralMax = 1;

    lanes[1].role = RhythmRole::BassRhythm;
    lanes[1].optional = static_cast<StepMask>(stepBit(4) | stepBit(8));
    lanes[1].structuralMin = 0;
    lanes[1].structuralMax = 1;

    lanes[2].role = RhythmRole::Backbeat;
    lanes[2].immutableAnchors = stepBit(4);
    lanes[2].structuralMin = 1;
    lanes[2].structuralMax = 1;

    relationships[0].source = RhythmRole::Kick;
    relationships[0].target = RhythmRole::BassRhythm;
    relationships[0].op = RelationshipOp::Coincide;
    relationships[0].strength = ConstraintStrength::Hard;
    relationships[0].scope = RelationshipScope::BarLocal;
    relationships[0].zoneMask =
        static_cast<StepMask>(stepBit(4) | stepBit(8));
    relationships[0].minMatches = 1;
    relationships[0].maxMatches = 1;

    // Step 4 is geometrically legal for Coincide but Bass cannot use it
    // because the immutable Backbeat occupies the same protected relation
    // zone. Step 8 remains a valid Coincide solution.
    relationships[1].source = RhythmRole::Backbeat;
    relationships[1].target = RhythmRole::BassRhythm;
    relationships[1].op = RelationshipOp::Exclude;
    relationships[1].strength = ConstraintStrength::Hard;
    relationships[1].scope = RelationshipScope::BarLocal;
    relationships[1].zoneMask = stepBit(4);

    ref.id = trajectory.id;
    ref.weight = 100;
    ref.allowedLevels = kAllRealizationLevels;

    archetype.id = 204;
    archetype.family = RhythmFamily::MachineSyncopation;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::BassRhythm) |
        rhythmRoleBit(RhythmRole::Backbeat));
    archetype.lanes = lanes;
    archetype.laneCount = 3;
    archetype.relationships = relationships;
    archetype.relationshipCount = 2;
    archetype.trajectories = &ref;
    archetype.trajectoryCount = 1;
    archetype.density = DensityContract{1, 3, 3, 0};
    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{};
    archetype.mutation.level[2] = MutationBudget{};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

void testCoincideRepairTriesAnotherLegalSiteTransactionally() {
  CoincideChoiceFixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint32_t seed = 0; seed < 256; ++seed) {
    RhythmRealizationRequest request{};
    request.catalog = &fixture.catalog;
    request.archetypeId = fixture.archetype.id;
    request.phraseBars = 1;
    request.level = RealizationLevel::P1Canonical;
    request.generation.projectSeed = seed;
    request.generation.phraseOrdinal = 9;

    const RhythmRealizationResult result = realizeRhythmPhrase(request);
    assert(result.status != RealizationStatus::InvalidConstraintSet);
    const PhraseOccupancy occupancy = structuralOccupancy(result.plan);
    assert(hardRelationshipsSatisfied(fixture.archetype, occupancy));

    const StepMask kick =
        occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)];
    const StepMask bass =
        occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)];
    const StepMask backbeat =
        occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Backbeat)];

    assert(kick == stepBit(8));
    assert(bass == stepBit(8));
    assert(backbeat == stepBit(4));
  }
}

}  // namespace

int main() {
  testCoincideRepairTriesAnotherLegalSiteTransactionally();
  return 0;
}
