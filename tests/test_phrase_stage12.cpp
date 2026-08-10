#include <cassert>
#include <cstdint>

#include "src/generation/phrase/phrase_evolution.h"

using namespace GroovePuterRhythm;

namespace {

constexpr LaneGrammar lane(RhythmRole role,
                           StepMask immutableAnchors,
                           StepMask canonicalAnchors,
                           StepMask preferred,
                           StepMask optional,
                           uint8_t structuralMin,
                           uint8_t structuralMax,
                           uint8_t ornamentMax) {
  LaneGrammar value{};
  value.role = role;
  value.immutableAnchors = immutableAnchors;
  value.canonicalAnchors = canonicalAnchors;
  value.preferred = preferred;
  value.optional = optional;
  value.structuralMin = structuralMin;
  value.structuralMax = structuralMax;
  value.ornamentMax = ornamentMax;
  return value;
}

constexpr MutationPolicy mutationPolicy() {
  MutationPolicy policy{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P1Canonical)] =
      MutationBudget{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)] =
      MutationBudget{2, 1, 0, 0,
                     static_cast<uint16_t>(AllowGhostConversion |
                                           AllowPreferredDrops |
                                           AllowReduction),
                     transformationIntentBit(TransformationIntent::Reduce)};
  policy.level[static_cast<uint8_t>(RealizationLevel::P3Transformation)] =
      MutationBudget{3, 3, 0, 0,
                     static_cast<uint16_t>(AllowOptionalAdds |
                                           AllowPreferredDrops |
                                           AllowReduction |
                                           AllowTurnaround |
                                           AllowBreak),
                     static_cast<TransformationIntentMask>(
                         transformationIntentBit(TransformationIntent::Reduce) |
                         transformationIntentBit(TransformationIntent::Turnaround) |
                         transformationIntentBit(TransformationIntent::Break))};
  return policy;
}

constexpr LaneGrammar kLanes[] = {
    lane(RhythmRole::Kick, stepBit(0), stepBit(8),
         static_cast<StepMask>(stepBit(6) | stepBit(14)),
         static_cast<StepMask>(stepBit(3) | stepBit(11)), 2, 4, 2),
    lane(RhythmRole::Backbeat, 0,
         static_cast<StepMask>(stepBit(4) | stepBit(12)),
         0, 0, 2, 2, 1),
    lane(RhythmRole::ClosedHat, 0,
         static_cast<StepMask>(stepBit(2) | stepBit(6) |
                               stepBit(10) | stepBit(14)),
         static_cast<StepMask>(stepBit(1) | stepBit(5) |
                               stepBit(9) | stepBit(13)),
         static_cast<StepMask>(stepBit(3) | stepBit(7) |
                               stepBit(11) | stepBit(15)),
         4, 6, 4),
};

constexpr LaneRelationship kRelationships[] = {
    {RhythmRole::Backbeat, RhythmRole::Kick, RelationshipOp::Exclude,
     ConstraintStrength::Hard, RelationshipScope::BarLocal,
     kAllSteps, 0, 0, 0, 0, 0, 0, 0},
};

constexpr BarTrajectory kTrajectories[] = {
    {1, 1, {BarFunction::Statement, BarFunction::Statement,
            BarFunction::Statement, BarFunction::Statement}},
    {2, 2, {BarFunction::Statement, BarFunction::Repeat,
            BarFunction::Statement, BarFunction::Statement}},
    {3, 2, {BarFunction::Statement, BarFunction::RepeatWithGhosts,
            BarFunction::Statement, BarFunction::Statement}},
    {5, 4, {BarFunction::Statement, BarFunction::Response,
            BarFunction::Repeat, BarFunction::Return}},
    {6, 4, {BarFunction::Statement, BarFunction::Repeat,
            BarFunction::Reduction, BarFunction::Return}},
    {7, 4, {BarFunction::Statement, BarFunction::Build,
            BarFunction::RepeatWithGhosts, BarFunction::Turnaround}},
};

constexpr TrajectoryRef kTrajectoryRefs[] = {
    {1, 100, kAllRealizationLevels},
    {2, 70, kAllRealizationLevels},
    {3, 30, static_cast<RealizationLevelMask>(
                realizationLevelBit(RealizationLevel::P2Variation) |
                realizationLevelBit(RealizationLevel::P3Transformation))},
    {5, 70, kAllRealizationLevels},
    {6, 30, static_cast<RealizationLevelMask>(
                realizationLevelBit(RealizationLevel::P2Variation) |
                realizationLevelBit(RealizationLevel::P3Transformation))},
    {7, 20, realizationLevelBit(RealizationLevel::P3Transformation)},
};

constexpr RhythmArchetype makeArchetype() {
  RhythmArchetype value{};
  value.id = 900;
  value.family = RhythmFamily::MachineSyncopation;
  value.allowedPhraseBars = static_cast<PhraseBarsMask>(
      phraseBarsBit(1) | phraseBarsBit(2) | phraseBarsBit(4));
  value.activeRoles = static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::Kick) |
      rhythmRoleBit(RhythmRole::Backbeat) |
      rhythmRoleBit(RhythmRole::ClosedHat));
  value.lanes = kLanes;
  value.laneCount = 3;
  value.relationships = kRelationships;
  value.relationshipCount = 1;
  value.trajectories = kTrajectoryRefs;
  value.trajectoryCount = 6;
  value.density = DensityContract{8, 10, 12, 6};
  value.mutation = mutationPolicy();
  return value;
}

constexpr RhythmArchetype kArchetypes[] = {makeArchetype()};
constexpr RhythmCatalogView kCatalog = {
    kArchetypes, 1, kTrajectories,
    static_cast<uint8_t>(sizeof(kTrajectories) / sizeof(kTrajectories[0]))};

PhraseEvolutionRequest request(uint8_t bars,
                               RealizationLevel level,
                               TrajectoryId trajectory) {
  PhraseEvolutionRequest value{};
  value.catalog = &kCatalog;
  value.archetypeId = 900;
  value.phraseBars = bars;
  value.level = level;
  value.generation.projectSeed = 0x12008BAAu;
  value.generation.phraseOrdinal = 17;
  value.requestedTrajectoryId = trajectory;
  value.roleIdentity.bass = BassRhythmId::KickAnswer;
  value.roleIdentity.chord = ChordRhythmId::SparseChordReply;
  value.roleIdentity.melodic = MelodicRhythmId::DelayedAnswer;
  value.roleIdentity.motif = MotifShapeId::CallResponse;
  return value;
}

bool equalRole(const RoleRhythmPlan& a, const RoleRhythmPlan& b) {
  return a.structural == b.structural && a.secondary == b.secondary &&
         a.ghosts == b.ghosts && a.shortGate == b.shortGate &&
         a.heldGate == b.heldGate && a.tieGate == b.tieGate &&
         a.accents == b.accents;
}

bool equalBar(const RhythmBarPlan& a, const RhythmBarPlan& b) {
  if (a.function != b.function) return false;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!equalRole(a.roles[role], b.roles[role])) return false;
  }
  return true;
}

bool equalIdentity(const PhraseRhythmIdentity& a,
                   const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId || a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) return false;
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) return false;
    }
  }
  return true;
}

void testFirstClassLengths() {
  const PhraseEvolutionResult one = evolveMultiBarPhrase(
      request(1, RealizationLevel::P1Canonical, 1));
  const PhraseEvolutionResult two = evolveMultiBarPhrase(
      request(2, RealizationLevel::P2Variation, 3));
  const PhraseEvolutionResult four = evolveMultiBarPhrase(
      request(4, RealizationLevel::P2Variation, 6));
  const PhraseEvolutionResult eight = evolveMultiBarPhrase(
      request(8, RealizationLevel::P3Transformation, 7));
  assert(one.status == PhraseEvolutionStatus::Ok && one.barCount == 1 &&
         one.segmentCount == 1);
  assert(two.status == PhraseEvolutionStatus::Ok && two.barCount == 2 &&
         two.segmentCount == 1);
  assert(four.status == PhraseEvolutionStatus::Ok && four.barCount == 4 &&
         four.segmentCount == 1);
  assert(eight.status == PhraseEvolutionStatus::Ok && eight.barCount == 8 &&
         eight.segmentCount == 2);
  assert(eight.segmentTrajectories[0] == 7 &&
         eight.segmentTrajectories[1] == 7);
  assert(eight.variationHistoryMask != 0);
  assert(eight.roleIdentity.bass == BassRhythmId::KickAnswer &&
         eight.roleIdentity.chord == ChordRhythmId::SparseChordReply &&
         eight.roleIdentity.melodic == MelodicRhythmId::DelayedAnswer &&
         eight.roleIdentity.motif == MotifShapeId::CallResponse);
}

void testEightBarsReuseOneRhythmIdentity() {
  const PhraseEvolutionResult result = evolveMultiBarPhrase(
      request(8, RealizationLevel::P2Variation, 5));
  assert(result.status == PhraseEvolutionStatus::Ok);
  assert(result.rhythmIdentity.archetypeId == 900);
  assert(result.rhythmIdentity.phraseBars == 4);
  assert(result.bars[0].function == BarFunction::Statement);
  assert(result.bars[4].function == BarFunction::Statement);
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    assert(result.rhythmIdentity.structuralCore[0][role] != 0 ||
           role >= 3);
  }
}

void testPLevelsPreserveIdentity() {
  const PhraseEvolutionResult p1 = evolveMultiBarPhrase(
      request(4, RealizationLevel::P1Canonical, 5));
  const PhraseEvolutionResult p2 = evolveMultiBarPhrase(
      request(4, RealizationLevel::P2Variation, 5));
  const PhraseEvolutionResult p3 = evolveMultiBarPhrase(
      request(4, RealizationLevel::P3Transformation, 5));
  assert(p1.status == PhraseEvolutionStatus::Ok &&
         p2.status == PhraseEvolutionStatus::Ok &&
         p3.status == PhraseEvolutionStatus::Ok);
  assert(equalIdentity(p1.rhythmIdentity, p2.rhythmIdentity));
  assert(equalIdentity(p1.rhythmIdentity, p3.rhythmIdentity));
}

void testDeterminismAndInvalidRequests() {
  const PhraseEvolutionRequest valid =
      request(8, RealizationLevel::P3Transformation, 7);
  const PhraseEvolutionResult first = evolveMultiBarPhrase(valid);
  const PhraseEvolutionResult second = evolveMultiBarPhrase(valid);
  assert(first.status == second.status && first.barCount == second.barCount &&
         first.variationHistoryMask == second.variationHistoryMask);
  for (uint8_t bar = 0; bar < first.barCount; ++bar) {
    assert(equalBar(first.bars[bar], second.bars[bar]));
  }

  PhraseEvolutionRequest invalid = valid;
  invalid.phraseBars = 3;
  assert(evolveMultiBarPhrase(invalid).status ==
         PhraseEvolutionStatus::InvalidRequest);
  invalid = valid;
  invalid.catalog = nullptr;
  assert(evolveMultiBarPhrase(invalid).status ==
         PhraseEvolutionStatus::InvalidRequest);
  invalid = valid;
  invalid.requestedTrajectoryId = 99;
  const PhraseEvolutionResult missing = evolveMultiBarPhrase(invalid);
  assert(missing.status == PhraseEvolutionStatus::CoreEvolutionFailed);
  assert(missing.barCount == 0);
}

}  // namespace

int main() {
  testFirstClassLengths();
  testEightBarsReuseOneRhythmIdentity();
  testPLevelsPreserveIdentity();
  testDeterminismAndInvalidRequests();
  return 0;
}
