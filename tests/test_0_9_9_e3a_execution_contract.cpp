#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/rhythm/rhythm_canonical_diff.h"

using namespace GroovePuterRhythm;

namespace {

constexpr StepMask bit(uint8_t step) { return stepBit(step); }
constexpr StepMask bits(uint8_t a, uint8_t b) {
  return static_cast<StepMask>(bit(a) | bit(b));
}
constexpr StepMask bits(uint8_t a, uint8_t b, uint8_t c) {
  return static_cast<StepMask>(bit(a) | bit(b) | bit(c));
}

bool roleEqual(const RoleRhythmPlan& lhs, const RoleRhythmPlan& rhs) {
  return lhs.structural == rhs.structural &&
         lhs.secondary == rhs.secondary &&
         lhs.ghosts == rhs.ghosts &&
         lhs.shortGate == rhs.shortGate &&
         lhs.heldGate == rhs.heldGate &&
         lhs.tieGate == rhs.tieGate &&
         lhs.accents == rhs.accents;
}

bool barEqual(const RhythmBarPlan& lhs, const RhythmBarPlan& rhs) {
  if (lhs.function != rhs.function) return false;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!roleEqual(lhs.roles[role], rhs.roles[role])) return false;
  }
  return true;
}

bool planEqual(const RhythmPhrasePlan& lhs, const RhythmPhrasePlan& rhs) {
  if (lhs.barCount != rhs.barCount ||
      lhs.trajectoryId != rhs.trajectoryId ||
      lhs.level != rhs.level ||
      lhs.intent != rhs.intent) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    if (!barEqual(lhs.bars[bar], rhs.bars[bar])) return false;
  }
  return true;
}

struct Fixture {
  LaneGrammar lanes[2]{};
  ProtectedSpace protectedSpaces[1]{};
  AnchorTransformRule transformRules[1]{};
  RhythmArchetype archetype{};

  Fixture() {
    lanes[0].role = RhythmRole::ClosedHat;
    lanes[0].canonicalAnchors = bit(0);
    lanes[0].preferred = static_cast<StepMask>(
        kAllSteps & ~bit(0) & ~bit(14));
    lanes[0].forbidden = bit(14);
    lanes[0].shortGate = bits(4, 7);
    lanes[0].heldGate = bits(5, 6);
    lanes[0].tieGate = bit(8);
    lanes[0].structuralMin = 1;
    lanes[0].structuralMax = 16;
    lanes[0].ornamentMax = 16;

    lanes[1].role = RhythmRole::Percussion;
    lanes[1].preferred = kAllSteps;
    lanes[1].shortGate = bit(2);
    lanes[1].structuralMin = 0;
    lanes[1].structuralMax = 16;
    lanes[1].ornamentMax = 16;

    protectedSpaces[0].steps = bit(12);
    protectedSpaces[0].affectedRoles =
        rhythmRoleBit(RhythmRole::ClosedHat);

    transformRules[0].role = RhythmRole::ClosedHat;
    transformRules[0].barFunction = BarFunction::Turnaround;
    transformRules[0].intent = TransformationIntent::Turnaround;
    transformRules[0].displaceableCanonical = bit(0);

    archetype.id = 930;
    archetype.family = RhythmFamily::MachineSyncopation;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::ClosedHat) |
        rhythmRoleBit(RhythmRole::Percussion));
    archetype.lanes = lanes;
    archetype.laneCount = 2;
    archetype.protectedSpaces = protectedSpaces;
    archetype.protectedSpaceCount = 1;
    archetype.anchorTransformRules = transformRules;
    archetype.anchorTransformRuleCount = 1;
    archetype.density.structuralMin = 1;
    archetype.density.structuralPreferred = 3;
    archetype.density.structuralMax = 32;
    archetype.density.ornamentMax = 16;
  }
};

RhythmPhrasePlan basePlan(const Fixture& fixture) {
  RhythmPhrasePlan plan{};
  plan.barCount = 1;
  plan.trajectoryId = kNoTrajectoryId;
  plan.level = RealizationLevel::P3Transformation;
  plan.intent = TransformationIntent::Auto;
  plan.bars[0].function = BarFunction::Statement;

  RoleRhythmPlan& hats =
      plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
  hats.structural = bits(0, 4, 8);
  hats.ghosts = bit(11);
  hats.accents = bit(8);

  RoleRhythmPlan& percussion =
      plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::Percussion)];
  percussion.structural = bit(2);

  const bool normalized = applyRhythmBarFunctionMutation(
      fixture.archetype, plan, 0, BarFunction::Statement, 0xE3A00001u);
  assert(normalized);
  assert(rhythmMutationPlanValid(fixture.archetype, plan));
  return plan;
}

RoleRhythmPlan& hats(RhythmPhrasePlan& plan) {
  return plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
}

const RoleRhythmPlan& hats(const RhythmPhrasePlan& plan) {
  return plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
}

void normalizeGates(const Fixture& fixture, RhythmPhrasePlan& plan) {
  const bool ok = applyRhythmBarFunctionMutation(
      fixture.archetype, plan, 0, BarFunction::Statement, 0xE3A00002u);
  assert(ok);
}

void assertRoundTrip(const Fixture& fixture,
                     const RhythmPhrasePlan& before,
                     const RhythmPhrasePlan& after,
                     const RhythmMutationDelta& expected) {
  RhythmMutationDelta deltas[4]{};
  CanonicalRhythmDiffStats stats{};
  const CanonicalRhythmDiffStatus status = canonicalRhythmBarDiff(
      fixture.archetype,
      before.bars[0],
      after.bars[0],
      BarFunction::Statement,
      TransformationIntent::Auto,
      deltas,
      4,
      stats);
  assert(status == CanonicalRhythmDiffStatus::Ok);
  assert(stats.deltaCount == 1);
  assert(deltas[0].operation == expected.operation);
  assert(deltas[0].role == expected.role);
  assert(deltas[0].sourceStep == expected.sourceStep);
  assert(deltas[0].targetStep == expected.targetStep);
  if (expected.operation == RhythmMutationOp::DROP) {
    assert(stats.drops == 1);
    assert(stats.displacements == 0);
    assert(stats.adds == 0);
    assert(stats.ghostAdds == 0);
    assert(stats.accentChanges == 0);
  } else {
    assert(expected.operation == RhythmMutationOp::DISPLACE);
    assert(stats.displacements == 1);
    assert(stats.drops == 0);
    assert(stats.adds == 0);
    assert(stats.ghostAdds == 0);
    assert(stats.accentChanges == 0);
  }
}

enum class TestOnsetKind : uint8_t {
  Structural,
  Secondary,
  Ghost,
};

void testDropCase(TestOnsetKind kind, bool accented) {
  Fixture fixture;
  RhythmPhrasePlan before = basePlan(fixture);
  uint8_t source = 4;
  if (kind == TestOnsetKind::Secondary) {
    source = 5;
    hats(before).secondary = static_cast<StepMask>(
        hats(before).secondary | bit(source));
  } else if (kind == TestOnsetKind::Ghost) {
    source = 11;
  }
  if (accented) {
    hats(before).accents = static_cast<StepMask>(
        hats(before).accents | bit(source));
  }
  normalizeGates(fixture, before);

  RhythmPhrasePlan expected = before;
  RoleRhythmPlan& expectedRole = hats(expected);
  if (kind == TestOnsetKind::Structural) {
    expectedRole.structural = static_cast<StepMask>(
        expectedRole.structural & ~bit(source));
  } else if (kind == TestOnsetKind::Secondary) {
    expectedRole.secondary = static_cast<StepMask>(
        expectedRole.secondary & ~bit(source));
  } else {
    expectedRole.ghosts = static_cast<StepMask>(
        expectedRole.ghosts & ~bit(source));
  }
  expectedRole.accents = static_cast<StepMask>(
      expectedRole.accents & ~bit(source));
  normalizeGates(fixture, expected);

  RhythmPhrasePlan actual = before;
  const RhythmMutationDelta delta{
      RhythmMutationOp::DROP,
      RhythmRole::ClosedHat,
      source,
      kNoMutationStep};
  const RhythmMutationApplyStatus status = applyRhythmMutationDelta(
      fixture.archetype,
      actual,
      0,
      BarFunction::Statement,
      TransformationIntent::Auto,
      delta);
  assert(status == RhythmMutationApplyStatus::Success);
  assert(planEqual(actual, expected));
  assert((hats(actual).accents & bit(source)) == 0);
  assertRoundTrip(fixture, before, actual, delta);
}

void testDisplaceCase(TestOnsetKind kind, bool accented) {
  assert(kind != TestOnsetKind::Ghost);
  Fixture fixture;
  RhythmPhrasePlan before = basePlan(fixture);
  uint8_t source = 4;
  uint8_t target = 6;
  if (kind == TestOnsetKind::Secondary) {
    source = 5;
    target = 7;
    hats(before).secondary = static_cast<StepMask>(
        hats(before).secondary | bit(source));
  }
  if (accented) {
    hats(before).accents = static_cast<StepMask>(
        hats(before).accents | bit(source));
  }
  normalizeGates(fixture, before);

  RhythmPhrasePlan expected = before;
  RoleRhythmPlan& expectedRole = hats(expected);
  if (kind == TestOnsetKind::Structural) {
    expectedRole.structural = static_cast<StepMask>(
        (expectedRole.structural & ~bit(source)) | bit(target));
  } else {
    expectedRole.secondary = static_cast<StepMask>(
        (expectedRole.secondary & ~bit(source)) | bit(target));
  }
  const bool sourceWasAccented =
      (expectedRole.accents & bit(source)) != 0;
  expectedRole.accents = static_cast<StepMask>(
      expectedRole.accents & ~bit(source) & ~bit(target));
  if (sourceWasAccented) {
    expectedRole.accents = static_cast<StepMask>(
        expectedRole.accents | bit(target));
  }
  normalizeGates(fixture, expected);

  RhythmPhrasePlan actual = before;
  const RhythmMutationDelta delta{
      RhythmMutationOp::DISPLACE,
      RhythmRole::ClosedHat,
      source,
      target};
  const RhythmMutationApplyStatus status = applyRhythmMutationDelta(
      fixture.archetype,
      actual,
      0,
      BarFunction::Statement,
      TransformationIntent::Auto,
      delta);
  assert(status == RhythmMutationApplyStatus::Success);
  assert(planEqual(actual, expected));
  assert((hats(actual).accents & bit(source)) == 0);
  assert(((hats(actual).accents & bit(target)) != 0) == accented);
  assertRoundTrip(fixture, before, actual, delta);
}

void expectFailureAtomic(const Fixture& fixture,
                         RhythmPhrasePlan plan,
                         uint8_t bar,
                         BarFunction function,
                         TransformationIntent intent,
                         const RhythmMutationDelta& delta,
                         RhythmMutationApplyStatus expectedStatus) {
  const RhythmPhrasePlan before = plan;
  const RhythmMutationApplyStatus status = applyRhythmMutationDelta(
      fixture.archetype, plan, bar, function, intent, delta);
  assert(status == expectedStatus);
  assert(planEqual(plan, before));
}

void testFailClosedCases() {
  Fixture fixture;
  const RhythmPhrasePlan base = basePlan(fixture);

  expectFailureAtomic(
      fixture, base, 1, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DROP, RhythmRole::ClosedHat, 4, kNoMutationStep},
      RhythmMutationApplyStatus::InvalidRequest);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DROP, RhythmRole::Count, 4, kNoMutationStep},
      RhythmMutationApplyStatus::InvalidDelta);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DROP, RhythmRole::ClosedHat, 4, 5},
      RhythmMutationApplyStatus::InvalidDelta);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::ADD, RhythmRole::ClosedHat, kNoMutationStep, 6},
      RhythmMutationApplyStatus::UnsupportedOperation);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DROP, RhythmRole::ClosedHat, 6, kNoMutationStep},
      RhythmMutationApplyStatus::InvalidSource);

  RhythmPhrasePlan multiple = base;
  hats(multiple).secondary = static_cast<StepMask>(
      hats(multiple).secondary | bit(4));
  assert(rhythmMutationPlanValid(fixture.archetype, multiple));
  expectFailureAtomic(
      fixture, multiple, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DROP, RhythmRole::ClosedHat, 4, kNoMutationStep},
      RhythmMutationApplyStatus::InvalidSource);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 11, 10},
      RhythmMutationApplyStatus::UnsupportedSourceKind);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 10, 9},
      RhythmMutationApplyStatus::InvalidSource);

  RhythmPhrasePlan occupied = base;
  hats(occupied).secondary = static_cast<StepMask>(
      hats(occupied).secondary | bit(5));
  normalizeGates(fixture, occupied);
  expectFailureAtomic(
      fixture, occupied, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 4, 5},
      RhythmMutationApplyStatus::OccupiedTarget);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 4, 7},
      RhythmMutationApplyStatus::InvalidDelta);

  RhythmPhrasePlan protectedTarget = base;
  hats(protectedTarget).structural = static_cast<StepMask>(
      hats(protectedTarget).structural | bit(10));
  normalizeGates(fixture, protectedTarget);
  expectFailureAtomic(
      fixture, protectedTarget, 0,
      BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 10, 12},
      RhythmMutationApplyStatus::GrammarRejected);

  RhythmPhrasePlan forbiddenTarget = base;
  hats(forbiddenTarget).structural = static_cast<StepMask>(
      hats(forbiddenTarget).structural | bit(13));
  normalizeGates(fixture, forbiddenTarget);
  expectFailureAtomic(
      fixture, forbiddenTarget, 0,
      BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 13, 14},
      RhythmMutationApplyStatus::GrammarRejected);

  const RhythmMutationDelta anchorTarget{
      RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 2, 0};
  RhythmPhrasePlan anchorPlan = base;
  hats(anchorPlan).structural = static_cast<StepMask>(
      hats(anchorPlan).structural | bit(2));
  normalizeGates(fixture, anchorPlan);
  assert(!rhythmMutationDisplacementGrammarLegal(
      fixture.archetype,
      anchorTarget,
      BarFunction::Statement,
      TransformationIntent::Auto));
  expectFailureAtomic(
      fixture, anchorPlan, 0,
      BarFunction::Statement, TransformationIntent::Auto,
      anchorTarget,
      RhythmMutationApplyStatus::OccupiedTarget);

  expectFailureAtomic(
      fixture, base, 0, BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DISPLACE, RhythmRole::ClosedHat, 0, 2},
      RhythmMutationApplyStatus::GrammarRejected);

  Fixture minimumFixture;
  minimumFixture.lanes[0].structuralMin = 3;
  RhythmPhrasePlan minimumPlan = basePlan(minimumFixture);
  expectFailureAtomic(
      minimumFixture, minimumPlan, 0,
      BarFunction::Statement, TransformationIntent::Auto,
      {RhythmMutationOp::DROP, RhythmRole::ClosedHat, 4, kNoMutationStep},
      RhythmMutationApplyStatus::InvalidResult);
}

void testLegacyBarFunctionControl() {
  Fixture fixture;
  MutationBudget& p3 = fixture.archetype.mutation.level[
      static_cast<uint8_t>(RealizationLevel::P3Transformation)];
  p3.maxDrops = 1;

  RhythmPhrasePlan reduction = basePlan(fixture);
  hats(reduction).secondary = static_cast<StepMask>(
      hats(reduction).secondary | bit(5));
  normalizeGates(fixture, reduction);
  assert(applyRhythmBarFunctionMutation(
      fixture.archetype, reduction, 0,
      BarFunction::Reduction, 0xE3A0B001u));
  assert(reduction.bars[0].function == BarFunction::Reduction);
  assert(hats(reduction).ghosts == 0);
  assert((hats(reduction).secondary & bit(5)) == 0);

  RhythmPhrasePlan breakPlan = basePlan(fixture);
  hats(breakPlan).secondary = static_cast<StepMask>(
      hats(breakPlan).secondary | bit(5));
  normalizeGates(fixture, breakPlan);
  assert(applyRhythmBarFunctionMutation(
      fixture.archetype, breakPlan, 0,
      BarFunction::Break, 0xE3A0B002u));
  assert(breakPlan.bars[0].function == BarFunction::Break);
  assert(hats(breakPlan).ghosts == 0);
  assert((hats(breakPlan).secondary & bit(5)) == 0);
}

}  // namespace

int main() {
  testDropCase(TestOnsetKind::Structural, false);
  testDropCase(TestOnsetKind::Secondary, false);
  testDropCase(TestOnsetKind::Ghost, false);
  testDropCase(TestOnsetKind::Structural, true);
  testDropCase(TestOnsetKind::Secondary, true);
  testDropCase(TestOnsetKind::Ghost, true);

  testDisplaceCase(TestOnsetKind::Structural, false);
  testDisplaceCase(TestOnsetKind::Secondary, false);
  testDisplaceCase(TestOnsetKind::Structural, true);
  testDisplaceCase(TestOnsetKind::Secondary, true);

  testFailClosedCases();
  testLegacyBarFunctionControl();

  std::cout << "0.9.9-E3a DROP/DISPLACE exact execution contract: OK\n";
  return 0;
}
