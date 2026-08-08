#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct SingleLaneIntentFixture {
  BarTrajectory trajectories[4]{};
  LaneGrammar lane{};
  TrajectoryRef refs[4]{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  SingleLaneIntentFixture() {
    trajectories[0].id = 1;
    trajectories[0].barCount = 1;
    trajectories[0].bars[0] = BarFunction::Statement;

    trajectories[1].id = 2;
    trajectories[1].barCount = 1;
    trajectories[1].bars[0] = BarFunction::Reduction;

    trajectories[2].id = 3;
    trajectories[2].barCount = 1;
    trajectories[2].bars[0] = BarFunction::Break;

    trajectories[3].id = 4;
    trajectories[3].barCount = 1;
    trajectories[3].bars[0] = BarFunction::Turnaround;

    lane.role = RhythmRole::Kick;
    lane.immutableAnchors = stepBit(0);
    lane.optional = stepBit(4);
    lane.structuralMin = 1;
    lane.structuralMax = 2;

    refs[0] = TrajectoryRef{
        trajectories[0].id, 100,
        realizationLevelBit(RealizationLevel::P1Canonical)};
    refs[1] = TrajectoryRef{
        trajectories[1].id, 100,
        realizationLevelBit(RealizationLevel::P2Variation)};
    refs[2] = TrajectoryRef{
        trajectories[2].id, 100,
        realizationLevelBit(RealizationLevel::P3Transformation)};
    refs[3] = TrajectoryRef{
        trajectories[3].id, 100,
        realizationLevelBit(RealizationLevel::P3Transformation)};

    archetype.id = 201;
    archetype.family = RhythmFamily::MachineSyncopation;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
    archetype.lanes = &lane;
    archetype.laneCount = 1;
    archetype.trajectories = refs;
    archetype.trajectoryCount = 4;
    archetype.density = DensityContract{1, 1, 2, 0};

    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{
        0, 0, 0, 0, AllowReduction,
        transformationIntentBit(TransformationIntent::Reduce)};
    archetype.mutation.level[2] = MutationBudget{
        0, 0, 0, 0,
        static_cast<uint16_t>(AllowBreak | AllowTurnaround),
        static_cast<TransformationIntentMask>(
            transformationIntentBit(TransformationIntent::Break) |
            transformationIntentBit(TransformationIntent::Turnaround))};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = trajectories;
    catalog.trajectoryCount = 4;
  }
};

RhythmRealizationResult realizeIntent(const SingleLaneIntentFixture& fixture,
                                      uint32_t seed,
                                      TransformationIntent intent) {
  RhythmRealizationRequest request{};
  request.catalog = &fixture.catalog;
  request.archetypeId = fixture.archetype.id;
  request.phraseBars = 1;
  request.level = RealizationLevel::P3Transformation;
  request.intent = intent;
  request.generation.projectSeed = seed;
  request.generation.phraseOrdinal = 2;
  return realizeRhythmPhrase(request);
}

void testExplicitIntentConstrainsTrajectorySelection() {
  SingleLaneIntentFixture fixture;
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint32_t seed = 0; seed < 256; ++seed) {
    const RhythmRealizationResult breakResult = realizeIntent(
        fixture, seed, TransformationIntent::Break);
    assert(breakResult.status != RealizationStatus::InvalidConstraintSet);
    assert(breakResult.plan.trajectoryId == 3);
    assert(breakResult.plan.bars[0].function == BarFunction::Break);

    const RhythmRealizationResult turnaroundResult = realizeIntent(
        fixture, seed, TransformationIntent::Turnaround);
    assert(turnaroundResult.status != RealizationStatus::InvalidConstraintSet);
    assert(turnaroundResult.plan.trajectoryId == 4);
    assert(turnaroundResult.plan.bars[0].function == BarFunction::Turnaround);
  }
}

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
    const PhraseOccupancy occupancy = structuralOccupancy(p2.plan);
    assert(occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::Kick)] ==
           stepBit(0));
  }
}

struct OrnamentBudgetFixture {
  BarTrajectory trajectories[2]{};
  LaneGrammar lanes[2]{};
  TrajectoryRef refs[2]{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};

  OrnamentBudgetFixture() {
    trajectories[0].id = 21;
    trajectories[0].barCount = 1;
    trajectories[0].bars[0] = BarFunction::Statement;
    trajectories[1].id = 22;
    trajectories[1].barCount = 1;
    trajectories[1].bars[0] = BarFunction::RepeatWithGhosts;

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

    refs[0] = TrajectoryRef{
        trajectories[0].id, 100,
        static_cast<RealizationLevelMask>(
            realizationLevelBit(RealizationLevel::P1Canonical) |
            realizationLevelBit(RealizationLevel::P3Transformation))};
    refs[1] = TrajectoryRef{
        trajectories[1].id, 100,
        realizationLevelBit(RealizationLevel::P2Variation)};

    archetype.id = 203;
    archetype.family = RhythmFamily::BrokenBeat;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::Backbeat));
    archetype.lanes = lanes;
    archetype.laneCount = 2;
    archetype.trajectories = refs;
    archetype.trajectoryCount = 2;
    archetype.density = DensityContract{2, 2, 2, 1};
    archetype.mutation.level[0] = MutationBudget{};
    archetype.mutation.level[1] = MutationBudget{
        2, 0, 0, 0, AllowGhostConversion, 0};
    archetype.mutation.level[2] = MutationBudget{};

    catalog.archetypes = &archetype;
    catalog.archetypeCount = 1;
    catalog.trajectories = trajectories;
    catalog.trajectoryCount = 2;
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
  testExplicitIntentConstrainsTrajectorySelection();
  testVariationStopsAtGlobalStructuralMax();
  testGhostVariationStopsAtGlobalOrnamentMax();
  return 0;
}
