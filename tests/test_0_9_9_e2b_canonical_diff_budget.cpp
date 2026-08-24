#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/rhythm/rhythm_canonical_diff.h"

using namespace GroovePuterRhythm;

namespace {

constexpr StepMask bits(uint8_t a) { return stepBit(a); }
constexpr StepMask bits(uint8_t a, uint8_t b) {
  return static_cast<StepMask>(stepBit(a) | stepBit(b));
}
constexpr StepMask bits(uint8_t a, uint8_t b, uint8_t c) {
  return static_cast<StepMask>(stepBit(a) | stepBit(b) | stepBit(c));
}

LaneGrammar lane(RhythmRole role,
                 StepMask canonical,
                 StepMask preferred,
                 StepMask forbidden,
                 uint8_t structuralMin) {
  LaneGrammar value{};
  value.role = role;
  value.canonicalAnchors = canonical;
  value.preferred = preferred;
  value.forbidden = forbidden;
  value.structuralMin = structuralMin;
  value.structuralMax = 16;
  value.ornamentMax = 16;
  return value;
}

struct Fixture {
  LaneGrammar lanes[3]{};
  ProtectedSpace spaces[1]{};
  LaneRelationship relationships[1]{};
  RhythmArchetype archetype{};

  Fixture() {
    lanes[0] = lane(
        RhythmRole::Kick,
        bits(0),
        static_cast<StepMask>(kAllSteps & ~bits(0) & ~bits(15)),
        bits(15), 1);
    lanes[1] = lane(
        RhythmRole::ClosedHat, 0, kAllSteps, 0, 0);
    lanes[2] = lane(
        RhythmRole::Percussion, 0, kAllSteps, 0, 0);

    spaces[0].steps = bits(12);
    spaces[0].affectedRoles = rhythmRoleBit(RhythmRole::Kick);

    archetype.id = 920;
    archetype.family = RhythmFamily::MachineSyncopation;
    archetype.allowedPhraseBars = phraseBarsBit(1) | phraseBarsBit(2);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::ClosedHat) |
        rhythmRoleBit(RhythmRole::Percussion));
    archetype.lanes = lanes;
    archetype.laneCount = 3;
    archetype.protectedSpaces = spaces;
    archetype.protectedSpaceCount = 1;
    archetype.density.structuralMin = 1;
    archetype.density.structuralPreferred = 1;
    archetype.density.structuralMax = 32;
    archetype.density.ornamentMax = 16;

    MutationBudget p2{};
    p2.maxAdds = 1;
    p2.maxDrops = 1;
    p2.maxDisplacements = 1;
    p2.maxAccentChanges = 1;
    p2.maxSecondaryAdds = 1;
    p2.maxGhostAdds = 1;
    p2.flags = static_cast<uint16_t>(
        AllowOptionalAdds | AllowPreferredDrops | AllowGhostConversion |
        AllowOptionalDisplace | AllowAccentVariation);
    archetype.mutation.level[
        static_cast<uint8_t>(RealizationLevel::P2Variation)] = p2;

    MutationBudget p3 = p2;
    p3.maxAdds = 2;
    p3.maxDrops = 2;
    p3.maxDisplacements = 2;
    p3.maxAccentChanges = 2;
    p3.maxSecondaryAdds = 2;
    p3.maxGhostAdds = 2;
    archetype.mutation.level[
        static_cast<uint8_t>(RealizationLevel::P3Transformation)] = p3;
  }
};

RhythmPhrasePlan canonicalPlan() {
  RhythmPhrasePlan plan{};
  plan.barCount = 1;
  plan.level = RealizationLevel::P1Canonical;
  plan.trajectoryId = kNoTrajectoryId;
  plan.intent = TransformationIntent::Auto;
  plan.bars[0].function = BarFunction::Statement;
  plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural =
      bits(0);
  return plan;
}

void normalizeGhostGates(RhythmPhrasePlan& plan) {
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      RoleRhythmPlan& value = plan.bars[bar].roles[role];
      value.shortGate = value.ghosts;
      value.heldGate = 0;
      value.tieGate = 0;
    }
  }
}

RhythmPhrasePlan candidateFrom(const RhythmPhrasePlan& canonical,
                               RealizationLevel level) {
  RhythmPhrasePlan candidate = canonical;
  candidate.level = level;
  candidate.bars[0].function = BarFunction::Statement;
  return candidate;
}

CanonicalRhythmCandidateValidation validate(
    const Fixture& fixture,
    const RhythmPhrasePlan& canonical,
    const RhythmPhrasePlan& candidate,
    RhythmMutationDelta* deltas,
    uint16_t capacity,
    RealizationLevel level = RealizationLevel::P2Variation) {
  return canonicalRhythmCandidateValid(
      fixture.archetype, canonical, candidate, 0, level,
      BarFunction::Statement, TransformationIntent::Auto,
      deltas, capacity);
}

void assertCanonicalOrder(const RhythmMutationDelta* deltas,
                          uint16_t count) {
  for (uint16_t i = 1; i < count; ++i) {
    assert(!rhythmMutationDeltaLess(deltas[i], deltas[i - 1]));
  }
}

void test_keep_add_drop_accent_ghost() {
  Fixture fixture;
  RhythmMutationDelta deltas[32]{};

  const RhythmPhrasePlan canonical = canonicalPlan();
  RhythmPhrasePlan same = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  auto result = validate(fixture, canonical, same, deltas, 32);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.deltaCount == 0);  // KEEP is the zero-delta material case.
  assert(result.legal);

  RhythmPhrasePlan added = same;
  added.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural |=
      bits(4);
  result = validate(fixture, canonical, added, deltas, 32);
  assert(result.legal && result.stats.adds == 1 &&
         result.stats.secondaryAdds == 0);
  assert(result.stats.deltaCount == 1);
  assert(deltas[0].operation == RhythmMutationOp::ADD);
  assert(deltas[0].targetStep == 4);

  RhythmPhrasePlan secondary = same;
  secondary.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary |=
      bits(4);
  result = validate(fixture, canonical, secondary, deltas, 32);
  assert(result.legal && result.stats.adds == 1 &&
         result.stats.secondaryAdds == 1);
  assert(deltas[0].operation == RhythmMutationOp::ADD);

  RhythmPhrasePlan withDropSource = canonical;
  withDropSource.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural |=
      bits(4);
  RhythmPhrasePlan dropped = candidateFrom(
      withDropSource, RealizationLevel::P2Variation);
  dropped.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural &=
      static_cast<StepMask>(~bits(4));
  result = validate(fixture, withDropSource, dropped, deltas, 32);
  assert(result.legal && result.stats.drops == 1);
  assert(deltas[0].operation == RhythmMutationOp::DROP);
  assert(deltas[0].sourceStep == 4);

  RhythmPhrasePlan accentedSource = canonical;
  accentedSource.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(6);
  RhythmPhrasePlan accented = candidateFrom(
      accentedSource, RealizationLevel::P2Variation);
  accented.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].accents =
      bits(6);
  result = validate(fixture, accentedSource, accented, deltas, 32);
  assert(result.legal && result.stats.accentChanges == 1);
  assert(deltas[0].operation == RhythmMutationOp::ACCENT);
  assert(deltas[0].sourceStep == 6 && deltas[0].targetStep == 6);

  RhythmPhrasePlan ghosted = same;
  ghosted.bars[0].roles[static_cast<uint8_t>(RhythmRole::Percussion)].ghosts =
      bits(7);
  normalizeGhostGates(ghosted);
  result = validate(fixture, canonical, ghosted, deltas, 32);
  assert(result.legal && result.stats.ghostAdds == 1);
  assert(deltas[0].operation == RhythmMutationOp::GHOST);
  assert(deltas[0].targetStep == 7);
}

void test_displacement_matching_and_ordering() {
  Fixture fixture;
  RhythmMutationDelta deltas[32]{};

  RhythmPhrasePlan canonical = canonicalPlan();
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(4);
  RhythmPhrasePlan moved = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  moved.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(6);

  auto result = validate(fixture, canonical, moved, deltas, 32);
  assert(result.legal && result.stats.displacements == 1);
  assert(result.stats.adds == 0 && result.stats.drops == 0);
  assert(deltas[0].operation == RhythmMutationOp::DISPLACE);
  assert(deltas[0].role == RhythmRole::ClosedHat);
  assert(deltas[0].sourceStep == 4 && deltas[0].targetStep == 6);

  // One source can reach both 5 and 7. E2c canonical target ordering chooses 5.
  canonical = canonicalPlan();
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(6);
  RhythmPhrasePlan ambiguous = candidateFrom(
      canonical, RealizationLevel::P3Transformation);
  ambiguous.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(5, 7);
  result = validate(
      fixture, canonical, ambiguous, deltas, 32,
      RealizationLevel::P3Transformation);
  assert(result.legal);
  assert(result.stats.displacements == 1 && result.stats.adds == 1);
  assert(deltas[0].operation == RhythmMutationOp::DISPLACE);
  assert(deltas[0].sourceStep == 6 && deltas[0].targetStep == 5);
  assert(deltas[1].operation == RhythmMutationOp::ADD &&
         deltas[1].targetStep == 7);
  assertCanonicalOrder(deltas, result.stats.deltaCount);

  // Mask storage order is reversed; output still follows logical step/role order.
  canonical = canonicalPlan();
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural |=
      bits(8);
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(10);
  RhythmPhrasePlan mixed = candidateFrom(
      canonical, RealizationLevel::P3Transformation);
  mixed.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].accents = bits(8);
  mixed.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = 0;
  mixed.bars[0].roles[static_cast<uint8_t>(RhythmRole::Percussion)].structural =
      bits(2);
  result = validate(
      fixture, canonical, mixed, deltas, 32,
      RealizationLevel::P3Transformation);
  assert(result.legal);
  assertCanonicalOrder(deltas, result.stats.deltaCount);
}

void test_no_wrap_radius_and_same_lane() {
  Fixture fixture;
  RhythmMutationDelta deltas[32]{};

  RhythmPhrasePlan canonical = canonicalPlan();
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(15);
  RhythmPhrasePlan wrap = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  wrap.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(0);
  auto result = validate(fixture, canonical, wrap, deltas, 32);
  assert(result.stats.displacements == 0);
  assert(result.stats.adds == 1 && result.stats.drops == 1);

  MutationPolicy displacementOnly{};
  MutationBudget& p2 = displacementOnly.level[
      static_cast<uint8_t>(RealizationLevel::P2Variation)];
  p2.maxDisplacements = 1;
  p2.flags = AllowOptionalDisplace;
  assert(!canonicalRhythmBudgetValid(
      displacementOnly, RealizationLevel::P2Variation,
      TransformationIntent::Auto, result.stats));

  canonical = canonicalPlan();
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(4);
  RhythmPhrasePlan radius3 = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  radius3.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(7);
  result = validate(fixture, canonical, radius3, deltas, 32);
  assert(result.stats.displacements == 0);
  assert(result.stats.adds == 1 && result.stats.drops == 1);
  assert(!canonicalRhythmBudgetValid(
      displacementOnly, RealizationLevel::P2Variation,
      TransformationIntent::Auto, result.stats));

  canonical = canonicalPlan();
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(4);
  RhythmPhrasePlan crossLane = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  crossLane.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = 0;
  crossLane.bars[0].roles[static_cast<uint8_t>(RhythmRole::Percussion)].structural =
      bits(5);
  result = validate(fixture, canonical, crossLane, deltas, 32);
  assert(result.stats.displacements == 0);
  assert(result.stats.adds == 1 && result.stats.drops == 1);
}

void test_budget_laundering_regressions() {
  Fixture fixture;
  RhythmMutationDelta deltas[32]{};

  // ADD: both local hops are legal, but the final candidate is two adds from C.
  RhythmPhrasePlan c = canonicalPlan();
  RhythmPhrasePlan a = candidateFrom(c, RealizationLevel::P2Variation);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary = bits(4);
  RhythmPhrasePlan b = a;
  b.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary |= bits(8);
  auto ca = validate(fixture, c, a, deltas, 32);
  auto ab = validate(fixture, a, b, deltas, 32);
  auto cb = validate(fixture, c, b, deltas, 32);
  assert(ca.legal && ab.legal);
  assert(cb.stats.secondaryAdds == 2 && !cb.budgetValid && !cb.legal);

  // DROP: resetting at A would hide the first canonical removal.
  c = canonicalPlan();
  c.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(4, 8);
  a = candidateFrom(c, RealizationLevel::P2Variation);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(8);
  b = a;
  b.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = 0;
  ca = validate(fixture, c, a, deltas, 32);
  ab = validate(fixture, a, b, deltas, 32);
  cb = validate(fixture, c, b, deltas, 32);
  assert(ca.legal && ab.legal);
  assert(cb.stats.drops == 2 && !cb.budgetValid && !cb.legal);

  // DISPLACE: 4->6 and 6->8 are each radius-2. Canonical 4->8 is not.
  c = canonicalPlan();
  c.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(4);
  a = candidateFrom(c, RealizationLevel::P2Variation);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(6);
  b = a;
  b.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(8);
  ca = validate(fixture, c, a, deltas, 32);
  ab = validate(fixture, a, b, deltas, 32);
  cb = validate(fixture, c, b, deltas, 32);
  assert(ca.legal && ab.legal);
  assert(cb.stats.displacements == 0 && cb.stats.adds == 1 && cb.stats.drops == 1);
  MutationPolicy displacementOnly{};
  MutationBudget& d = displacementOnly.level[
      static_cast<uint8_t>(RealizationLevel::P2Variation)];
  d.maxDisplacements = 1;
  d.flags = AllowOptionalDisplace;
  assert(!canonicalRhythmBudgetValid(
      displacementOnly, RealizationLevel::P2Variation,
      TransformationIntent::Auto, cb.stats));

  // ACCENT: two canonical-relative accent changes cannot be reset to one at A.
  c = canonicalPlan();
  c.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(4, 8);
  a = candidateFrom(c, RealizationLevel::P2Variation);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].accents = bits(4);
  b = a;
  b.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].accents |= bits(8);
  ca = validate(fixture, c, a, deltas, 32);
  ab = validate(fixture, a, b, deltas, 32);
  cb = validate(fixture, c, b, deltas, 32);
  assert(ca.legal && ab.legal);
  assert(cb.stats.accentChanges == 2 && !cb.budgetValid && !cb.legal);

  // GHOST: same canonical-relative rule for ornament additions.
  c = canonicalPlan();
  a = candidateFrom(c, RealizationLevel::P2Variation);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::Percussion)].ghosts = bits(5);
  normalizeGhostGates(a);
  b = a;
  b.bars[0].roles[static_cast<uint8_t>(RhythmRole::Percussion)].ghosts |= bits(7);
  normalizeGhostGates(b);
  ca = validate(fixture, c, a, deltas, 32);
  ab = validate(fixture, a, b, deltas, 32);
  cb = validate(fixture, c, b, deltas, 32);
  assert(ca.legal && ab.legal);
  assert(cb.stats.ghostAdds == 2 && !cb.budgetValid && !cb.legal);

  // Mixed: one accent plus two secondary additions still exposes the second ADD.
  c = canonicalPlan();
  c.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(4);
  a = candidateFrom(c, RealizationLevel::P2Variation);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].accents = bits(4);
  a.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary = bits(6);
  b = a;
  b.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary |= bits(8);
  ab = validate(fixture, a, b, deltas, 32);
  cb = validate(fixture, c, b, deltas, 32);
  assert(ab.legal);
  assert(cb.stats.accentChanges == 1 && cb.stats.secondaryAdds == 2);
  assert(!cb.budgetValid && !cb.legal);
}

void test_existing_music_validation_is_reused() {
  Fixture fixture;
  RhythmMutationDelta deltas[32]{};
  const RhythmPhrasePlan canonical = canonicalPlan();

  // Canonical anchor removal: budget can afford the DROP, realizer identity
  // constraints reject the candidate.
  RhythmPhrasePlan anchorDrop = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  anchorDrop.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural = 0;
  auto result = validate(fixture, canonical, anchorDrop, deltas, 32);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.drops == 1 && result.budgetValid);
  assert(!result.candidatePlanValid && !result.legal);

  // Protected destination is rejected by the existing plan validator.
  RhythmPhrasePlan protectedAdd = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  protectedAdd.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural |=
      bits(12);
  result = validate(fixture, canonical, protectedAdd, deltas, 32);
  assert(result.stats.adds == 1 && result.budgetValid);
  assert(!result.candidatePlanValid && !result.legal);

  // Forbidden destination is likewise not reimplemented in E2b.
  RhythmPhrasePlan forbiddenAdd = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  forbiddenAdd.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural |=
      bits(15);
  result = validate(fixture, canonical, forbiddenAdd, deltas, 32);
  assert(result.stats.adds == 1 && result.budgetValid);
  assert(!result.candidatePlanValid && !result.legal);

  // Material in a role outside the archetype is an identity violation.
  RhythmPhrasePlan wrongRole = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  wrongRole.bars[0].roles[static_cast<uint8_t>(RhythmRole::BassRhythm)].structural =
      bits(4);
  result = validate(fixture, canonical, wrongRole, deltas, 32);
  assert(!result.candidatePlanValid && !result.legal);

  // Hard relationship rejection remains relationship_resolver ownership.
  fixture.relationships[0].source = RhythmRole::ClosedHat;
  fixture.relationships[0].target = RhythmRole::Percussion;
  fixture.relationships[0].op = RelationshipOp::Exclude;
  fixture.relationships[0].strength = ConstraintStrength::Hard;
  fixture.relationships[0].scope = RelationshipScope::BarLocal;
  fixture.relationships[0].zoneMask = kAllSteps;
  fixture.archetype.relationships = fixture.relationships;
  fixture.archetype.relationshipCount = 1;

  RhythmPhrasePlan relationCanonical = canonicalPlan();
  relationCanonical.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(4);
  RhythmPhrasePlan relationCandidate = candidateFrom(
      relationCanonical, RealizationLevel::P2Variation);
  relationCandidate.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::Percussion)].secondary = bits(4);
  result = validate(
      fixture, relationCanonical, relationCandidate, deltas, 32);
  assert(result.stats.secondaryAdds == 1 && result.budgetValid);
  assert(!result.candidatePlanValid && !result.legal);
}

void test_fail_closed_and_policy_levels() {
  Fixture fixture;
  RhythmMutationDelta deltas[32]{};
  RhythmPhrasePlan canonical = canonicalPlan();

  // Same-site importance conversion has no E2c operation and fails closed.
  canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)].structural =
      bits(4);
  RhythmPhrasePlan converted = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  converted.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = 0;
  converted.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::ClosedHat)].secondary = bits(4);
  auto result = validate(fixture, canonical, converted, deltas, 32);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::UnrepresentableDelta);
  assert(!result.legal);

  // Overlapping onset classes are invalid candidate material.
  converted = candidateFrom(canonical, RealizationLevel::P2Variation);
  converted.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::ClosedHat)].secondary = bits(4);
  result = validate(fixture, canonical, converted, deltas, 32);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::InvalidCandidateMaterial);
  assert(!result.legal);

  // Stats-only mode is supported; undersized requested output fails closed.
  CanonicalRhythmDiffStats stats{};
  RhythmPhrasePlan radius = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  radius.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::ClosedHat)].structural = bits(7);
  auto status = canonicalRhythmBarDiff(
      fixture.archetype, canonical.bars[0], radius.bars[0],
      BarFunction::Statement, TransformationIntent::Auto,
      nullptr, 0, stats);
  assert(status == CanonicalRhythmDiffStatus::Ok && stats.deltaCount == 2);
  status = canonicalRhythmBarDiff(
      fixture.archetype, canonical.bars[0], radius.bars[0],
      BarFunction::Statement, TransformationIntent::Auto,
      deltas, 1, stats);
  assert(status == CanonicalRhythmDiffStatus::OutputTooSmall);

  // P1/P2/P3 remain bounded under the existing MutationPolicy levels.
  canonical = canonicalPlan();
  RhythmPhrasePlan one = candidateFrom(
      canonical, RealizationLevel::P2Variation);
  one.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary = bits(4);
  status = canonicalRhythmBarDiff(
      fixture.archetype, canonical.bars[0], one.bars[0],
      BarFunction::Statement, TransformationIntent::Auto,
      nullptr, 0, stats);
  assert(status == CanonicalRhythmDiffStatus::Ok);
  assert(!canonicalRhythmBudgetValid(
      fixture.archetype.mutation, RealizationLevel::P1Canonical,
      TransformationIntent::Auto, stats));
  assert(canonicalRhythmBudgetValid(
      fixture.archetype.mutation, RealizationLevel::P2Variation,
      TransformationIntent::Auto, stats));
  assert(canonicalRhythmBudgetValid(
      fixture.archetype.mutation, RealizationLevel::P3Transformation,
      TransformationIntent::Auto, stats));

  RhythmPhrasePlan two = one;
  two.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary |= bits(8);
  status = canonicalRhythmBarDiff(
      fixture.archetype, canonical.bars[0], two.bars[0],
      BarFunction::Statement, TransformationIntent::Auto,
      nullptr, 0, stats);
  assert(status == CanonicalRhythmDiffStatus::Ok);
  assert(!canonicalRhythmBudgetValid(
      fixture.archetype.mutation, RealizationLevel::P2Variation,
      TransformationIntent::Auto, stats));
  assert(canonicalRhythmBudgetValid(
      fixture.archetype.mutation, RealizationLevel::P3Transformation,
      TransformationIntent::Auto, stats));

  RhythmPhrasePlan three = two;
  three.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].secondary |= bits(10);
  status = canonicalRhythmBarDiff(
      fixture.archetype, canonical.bars[0], three.bars[0],
      BarFunction::Statement, TransformationIntent::Auto,
      nullptr, 0, stats);
  assert(status == CanonicalRhythmDiffStatus::Ok);
  assert(!canonicalRhythmBudgetValid(
      fixture.archetype.mutation, RealizationLevel::P3Transformation,
      TransformationIntent::Auto, stats));

  // Preserve E1a P3 cumulative P2 ghost allowance when P3 declares none.
  MutationPolicy cumulative{};
  MutationBudget& p2 = cumulative.level[
      static_cast<uint8_t>(RealizationLevel::P2Variation)];
  p2.maxAdds = 1;
  p2.flags = AllowGhostConversion;
  MutationBudget& p3 = cumulative.level[
      static_cast<uint8_t>(RealizationLevel::P3Transformation)];
  p3.maxAdds = 1;
  p3.flags = AllowOptionalAdds;
  CanonicalRhythmDiffStats ghostStats{};
  ghostStats.deltaCount = 1;
  ghostStats.ghostAdds = 1;
  assert(canonicalRhythmBudgetValid(
      cumulative, RealizationLevel::P3Transformation,
      TransformationIntent::Auto, ghostStats));
}

}  // namespace

int main() {
  test_keep_add_drop_accent_ghost();
  test_displacement_matching_and_ordering();
  test_no_wrap_radius_and_same_lane();
  test_budget_laundering_regressions();
  test_existing_music_validation_is_reused();
  test_fail_closed_and_policy_levels();
  std::cout << "0.9.9-E2b canonical rhythm diff / budget tests: PASS\n";
  return 0;
}
