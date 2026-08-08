#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct StructuralBudgetFixture {
  BarTrajectory trajectory{};
  LaneGrammar lane{};
  TrajectoryRef ref{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  StructuralBudgetFixture() {
    trajectory.id = 11;
    trajectory.barCount = 1;
    trajectory.bars[0] = BarFunction::Statement;

    lane.role = RhythmRole::Kick;
    lane.immutableAnchors = stepBit(0);
    lane.optional = stepBit(4);
    lane.structuralMin = 1;
    lane.structuralMax = 2;

    ref.id = trajectory.id;
    ref.weight = 100;
    ref.allowedLevels = kAllRealizationLevels;

    archetype.id = 202;
    archetype.family = RhythmFamily::FourFloor;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
    archetype.lanes = &lane;
    archetype.laneCount = 1;
    archetype.trajectories = &ref;
    archetype.trajectoryCount = 1;
    archetype.density = DensityContract{1, 1, 1, 0};
    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{
        1, 0, 0, 0, AllowOptionalAdds, 0};
    archetype.mutation.level[2] = MutationBudget{
        1, 0, 0, 0, AllowOptionalAdds, 0};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

void testVariationStopsAtGlobalStructuralMax() {
  StructuralBudgetFixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint32_t seed = 0; seed < 64; ++seed) {
    RhythmRealizationRequest p1Request{};
    p1Request.catalog = &fixture.catalog;
    p1Request.archetypeId = fixture.archetype.id;
    p1Request.phraseBars = 1;
    p1Request.level = RealizationLevel::P1Canonical;
    p1Request.generation.projectSeed = seed;
    const RhythmRealizationResult p1 = realizeRhythmPhrase(p1Request);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);

    RhythmRealizationRequest p2Request = p1Request;
    p2Request.level = RealizationLevel::P2Variation;
    p2Request.reuseIdentity = &p1.identity;
    const RhythmRealizationResult p2 = realizeRhythmPhrase(p2Request);
    assert(p2.status != RealizationStatus::InvalidConstraintSet);
    assert(p2.plan.trajectoryId == kNoTrajectoryId);
    assert(p2.plan.bars[0].function == BarFunction::Statement);
    const PhraseOccupancy occupancy = structuralOccupancy(p2.plan);
    assert(occupancy.roleMasks[0]
               [static_cast<uint8_t>(RhythmRole::Kick)] == stepBit(0));
  }
}

struct OrnamentBudgetFixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[2]{};
  TrajectoryRef ref{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  OrnamentBudgetFixture() {
    trajectory.id = 21;
    trajectory.barCount = 1;
    trajectory.bars[0] = BarFunction::Statement;

    lanes[0].role = RhythmRole::Kick;
    lanes[0].immutableAnchors = stepBit(0);
    lanes[0].optional = stepBit(2);
    lanes[0].structuralMin = 1;
    lanes[0].structuralMax = 1;
    lanes[0].ornamentMax = 1;

    lanes[1].role = RhythmRole::Backbeat;
    lanes[1].immutableAnchors = stepBit(8);
    lanes[1].optional = stepBit(6);
    lanes[1].structuralMin = 1;
    lanes[1].structuralMax = 1;
    lanes[1].ornamentMax = 1;

    ref.id = trajectory.id;
    ref.weight = 100;
    ref.allowedLevels = kAllRealizationLevels;

    archetype.id = 203;
    archetype.family = RhythmFamily::Breakbeat;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::Backbeat));
    archetype.lanes = lanes;
    archetype.laneCount = 2;
    archetype.trajectories = &ref;
    archetype.trajectoryCount = 1;
    archetype.density = DensityContract{2, 2, 2, 1};
    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{
        2, 0, 0, 0, AllowGhostConversion, 0};
    archetype.mutation.level[2] = MutationBudget{
        2, 0, 0, 0, AllowGhostConversion, 0};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = &trajectory;
    catalog.trajectoryCount = 1;
  }
};

uint8_t bitCount(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

void testGhostVariationStopsAtGlobalOrnamentMax() {
  OrnamentBudgetFixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint32_t seed = 0; seed < 64; ++seed) {
    RhythmRealizationRequest p1Request{};
    p1Request.catalog = &fixture.catalog;
    p1Request.archetypeId = fixture.archetype.id;
    p1Request.phraseBars = 1;
    p1Request.level = RealizationLevel::P1Canonical;
    p1Request.generation.projectSeed = seed;
    const RhythmRealizationResult p1 = realizeRhythmPhrase(p1Request);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);

    RhythmRealizationRequest p2Request = p1Request;
    p2Request.level = RealizationLevel::P2Variation;
    p2Request.reuseIdentity = &p1.identity;
    const RhythmRealizationResult p2 = realizeRhythmPhrase(p2Request);
    assert(p2.status != RealizationStatus::InvalidConstraintSet);
    assert(p2.plan.trajectoryId == kNoTrajectoryId);
    assert(p2.plan.bars[0].function == BarFunction::Statement);

    uint8_t ornaments = 0;
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      ornaments = static_cast<uint8_t>(
          ornaments + bitCount(p2.plan.bars[0].roles[role].ghosts));
    }
    assert(ornaments <= 1);
  }
}

}  // namespace

int main() {
  testVariationStopsAtGlobalStructuralMax();
  testGhostVariationStopsAtGlobalOrnamentMax();
  return 0;
}
