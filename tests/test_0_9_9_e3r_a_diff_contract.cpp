#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/rhythm/rhythm_canonical_diff.h"

using namespace GroovePuterRhythm;

namespace {

constexpr StepMask bit(uint8_t step) { return stepBit(step); }

struct Fixture {
  LaneGrammar lanes[2]{};
  RhythmArchetype archetype{};

  Fixture() {
    lanes[0].role = RhythmRole::Kick;
    lanes[0].canonicalAnchors = bit(0);
    lanes[0].preferred = static_cast<StepMask>(kAllSteps & ~bit(0));
    lanes[0].structuralMin = 1;
    lanes[0].structuralMax = 16;
    lanes[0].ornamentMax = 16;

    lanes[1].role = RhythmRole::ClosedHat;
    lanes[1].preferred = kAllSteps;
    lanes[1].shortGate = bit(6);
    lanes[1].heldGate = bit(4);
    lanes[1].tieGate = bit(8);
    lanes[1].structuralMin = 1;
    lanes[1].structuralMax = 16;
    lanes[1].ornamentMax = 16;

    archetype.id = 930;
    archetype.family = RhythmFamily::MachineSyncopation;
    archetype.allowedPhraseBars = phraseBarsBit(1);
    archetype.activeRoles = static_cast<RhythmRoleMask>(
        rhythmRoleBit(RhythmRole::Kick) |
        rhythmRoleBit(RhythmRole::ClosedHat));
    archetype.lanes = lanes;
    archetype.laneCount = 2;
    archetype.density.structuralMin = 2;
    archetype.density.structuralPreferred = 2;
    archetype.density.structuralMax = 32;
    archetype.density.ornamentMax = 16;

    MutationBudget& p2 = archetype.mutation.level[
        static_cast<uint8_t>(RealizationLevel::P2Variation)];
    p2.maxDrops = 1;
    p2.maxDisplacements = 1;
    p2.flags = static_cast<uint16_t>(
        AllowPreferredDrops | AllowOptionalDisplace);
  }
};

RhythmPhrasePlan canonicalPlan() {
  RhythmPhrasePlan plan{};
  plan.barCount = 1;
  plan.level = RealizationLevel::P1Canonical;
  plan.trajectoryId = kNoTrajectoryId;
  plan.intent = TransformationIntent::Auto;
  plan.bars[0].function = BarFunction::Statement;

  RoleRhythmPlan& kick =
      plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)];
  kick.structural = bit(0);

  RoleRhythmPlan& hat =
      plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
  hat.structural = static_cast<StepMask>(bit(4) | bit(8));
  hat.heldGate = bit(4);
  hat.tieGate = bit(8);
  hat.accents = bit(4);
  return plan;
}

RhythmPhrasePlan p2Candidate(const RhythmPhrasePlan& canonical) {
  RhythmPhrasePlan candidate = canonical;
  candidate.level = RealizationLevel::P2Variation;
  candidate.bars[0].function = BarFunction::Statement;
  return candidate;
}

CanonicalRhythmCandidateValidation validate(
    const Fixture& fixture,
    const RhythmPhrasePlan& canonical,
    const RhythmPhrasePlan& candidate,
    RhythmMutationDelta* deltas,
    uint16_t capacity) {
  return canonicalRhythmCandidateValid(
      fixture.archetype, canonical, candidate, 0,
      RealizationLevel::P2Variation,
      BarFunction::Statement, TransformationIntent::Auto,
      deltas, capacity);
}

void test_drop_diff_contract_only() {
  Fixture fixture;
  const RhythmPhrasePlan canonical = canonicalPlan();
  RhythmMutationDelta deltas[8]{};

  // DIFF CONTRACT TEST ONLY. W is hand-constructed. This deliberately does not
  // claim or emulate an authoritative DROP materializer.
  RhythmPhrasePlan dropped = p2Candidate(canonical);
  RoleRhythmPlan& droppedHat =
      dropped.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
  droppedHat.structural = bit(8);
  droppedHat.heldGate = 0;
  droppedHat.tieGate = bit(8);
  droppedHat.shortGate = 0;
  droppedHat.accents = 0;

  auto result = validate(fixture, canonical, dropped, deltas, 8);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.deltaCount == 1);
  assert(result.stats.drops == 1);
  assert(result.stats.displacements == 0);
  assert(result.stats.accentChanges == 0);
  assert(deltas[0].operation == RhythmMutationOp::DROP);
  assert(deltas[0].role == RhythmRole::ClosedHat);
  assert(deltas[0].sourceStep == 4);
  assert(deltas[0].targetStep == kNoMutationStep);
  assert(result.canonicalPlanValid);
  assert(result.candidatePlanValid);
  assert(result.budgetValid);
  assert(result.legal);

  // The structural validator owns lane-derived gate consistency. A stale gate
  // mask is not accepted even though the onset-level diff itself is a DROP.
  RhythmPhrasePlan staleGates = dropped;
  staleGates.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)]
      .heldGate = bit(4);
  result = validate(fixture, canonical, staleGates, deltas, 8);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.drops == 1);
  assert(!result.candidatePlanValid);
  assert(!result.legal);

  // E2b rejects a dangling accent as invalid candidate material. This proves a
  // consumer constraint, not whether a future DROP executor must clear or
  // reject such a source; that materialization policy remains undefined.
  RhythmPhrasePlan danglingAccent = dropped;
  danglingAccent.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)]
      .accents = bit(4);
  result = validate(fixture, canonical, danglingAccent, deltas, 8);
  assert(result.diffStatus ==
         CanonicalRhythmDiffStatus::InvalidCandidateMaterial);
  assert(!result.legal);
}

void test_displace_diff_contract_only() {
  Fixture fixture;
  const RhythmPhrasePlan canonical = canonicalPlan();
  RhythmMutationDelta deltas[8]{};

  // DIFF CONTRACT TEST ONLY. W is hand-constructed. It demonstrates E2b
  // recognition plus structural validation, not execution semantics.
  RhythmPhrasePlan moved = p2Candidate(canonical);
  RoleRhythmPlan& movedHat =
      moved.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
  movedHat.structural = static_cast<StepMask>(bit(6) | bit(8));
  movedHat.shortGate = bit(6);
  movedHat.heldGate = 0;
  movedHat.tieGate = bit(8);
  movedHat.accents = bit(6);

  auto result = validate(fixture, canonical, moved, deltas, 8);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.deltaCount == 1);
  assert(result.stats.displacements == 1);
  assert(result.stats.adds == 0);
  assert(result.stats.drops == 0);
  assert(result.stats.accentChanges == 0);
  assert(deltas[0].operation == RhythmMutationOp::DISPLACE);
  assert(deltas[0].role == RhythmRole::ClosedHat);
  assert(deltas[0].sourceStep == 4);
  assert(deltas[0].targetStep == 6);
  assert(result.canonicalPlanValid);
  assert(result.candidatePlanValid);
  assert(result.budgetValid);
  assert(result.legal);

  // E2b's matching requires equal accent state on source/target. Clearing the
  // source accent rather than carrying equal state to the target is therefore
  // observed as DROP + ADD, not as DISPLACE. This is diff semantics only.
  RhythmPhrasePlan accentNotMoved = moved;
  accentNotMoved.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)]
      .accents = 0;
  result = validate(fixture, canonical, accentNotMoved, deltas, 8);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.displacements == 0);
  assert(result.stats.drops == 1);
  assert(result.stats.adds == 1);
  assert(!result.budgetValid);
  assert(!result.legal);

  RhythmPhrasePlan staleGates = moved;
  RoleRhythmPlan& staleHat =
      staleGates.bars[0].roles[static_cast<uint8_t>(RhythmRole::ClosedHat)];
  staleHat.shortGate = 0;
  staleHat.heldGate = bit(4);
  result = validate(fixture, canonical, staleGates, deltas, 8);
  assert(result.diffStatus == CanonicalRhythmDiffStatus::Ok);
  assert(result.stats.displacements == 1);
  assert(!result.candidatePlanValid);
  assert(!result.legal);
}

}  // namespace

int main() {
  test_drop_diff_contract_only();
  test_displace_diff_contract_only();
  std::cout << "0.9.9-E3R-A DROP/DISPLACE diff-contract audit: OK\n";
  return 0;
}
