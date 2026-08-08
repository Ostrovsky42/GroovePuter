#include "reference_vocabulary.h"

namespace GroovePuterRhythm {
namespace ReferenceVocabulary {
namespace {

constexpr LaneGrammar lane(RhythmRole role,
                           StepMask immutableAnchors,
                           StepMask canonicalAnchors,
                           StepMask preferred,
                           StepMask optional,
                           uint8_t structuralMin,
                           uint8_t structuralMax,
                           uint8_t ornamentMax = 1,
                           StepMask shortGate = 0,
                           StepMask heldGate = 0,
                           StepMask tieGate = 0) {
  LaneGrammar value{};
  value.role = role;
  value.immutableAnchors = immutableAnchors;
  value.canonicalAnchors = canonicalAnchors;
  value.preferred = preferred;
  value.optional = optional;
  value.shortGate = shortGate;
  value.heldGate = heldGate;
  value.tieGate = tieGate;
  value.structuralMin = structuralMin;
  value.structuralMax = structuralMax;
  value.ornamentMax = ornamentMax;
  return value;
}

constexpr LaneRelationship hardCoincide(RhythmRole source,
                                         RhythmRole target,
                                         uint8_t minMatches,
                                         uint8_t maxMatches) {
  LaneRelationship value{};
  value.source = source;
  value.target = target;
  value.op = RelationshipOp::Coincide;
  value.strength = ConstraintStrength::Hard;
  value.scope = RelationshipScope::BarLocal;
  value.zoneMask = kAllSteps;
  value.minMatches = minMatches;
  value.maxMatches = maxMatches;
  return value;
}

constexpr LaneRelationship hardExclude(RhythmRole source,
                                        RhythmRole target) {
  LaneRelationship value{};
  value.source = source;
  value.target = target;
  value.op = RelationshipOp::Exclude;
  value.strength = ConstraintStrength::Hard;
  value.scope = RelationshipScope::BarLocal;
  value.zoneMask = kAllSteps;
  return value;
}

constexpr LaneRelationship hardOffset(RhythmRole source,
                                       RhythmRole target,
                                       int8_t minOffset,
                                       int8_t maxOffset) {
  LaneRelationship value{};
  value.source = source;
  value.target = target;
  value.op = RelationshipOp::Offset;
  value.strength = ConstraintStrength::Hard;
  value.scope = RelationshipScope::BarLocal;
  value.zoneMask = kAllSteps;
  value.minOffset = minOffset;
  value.maxOffset = maxOffset;
  return value;
}

constexpr LaneRelationship softRespond(RhythmRole source,
                                        RhythmRole target,
                                        int8_t minOffset,
                                        int8_t maxOffset,
                                        uint8_t weight) {
  LaneRelationship value{};
  value.source = source;
  value.target = target;
  value.op = RelationshipOp::Respond;
  value.strength = ConstraintStrength::Soft;
  value.scope = RelationshipScope::BarLocal;
  value.zoneMask = kAllSteps;
  value.minOffset = minOffset;
  value.maxOffset = maxOffset;
  value.weight = weight;
  return value;
}

constexpr LaneRelationship softFillGaps(RhythmRole source,
                                         RhythmRole target,
                                         uint8_t weight) {
  LaneRelationship value{};
  value.source = source;
  value.target = target;
  value.op = RelationshipOp::FillGaps;
  value.strength = ConstraintStrength::Soft;
  value.scope = RelationshipScope::BarLocal;
  value.zoneMask = kAllSteps;
  value.weight = weight;
  return value;
}

constexpr ProtectedSpace protectedSpace(StepMask steps,
                                         RhythmRoleMask roles) {
  ProtectedSpace value{};
  value.steps = steps;
  value.affectedRoles = roles;
  return value;
}

constexpr TimingEligibility timing(TimingCompatibility compatibility,
                                   StepMask sensitiveSteps,
                                   RhythmRoleMask affectedRoles) {
  TimingEligibility value{};
  value.compatibility = compatibility;
  value.sensitiveSteps = sensitiveSteps;
  value.affectedRoles = affectedRoles;
  return value;
}

constexpr MutationPolicy referenceMutationPolicy() {
  MutationPolicy policy{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P1Canonical)] =
      MutationBudget{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)] =
      MutationBudget{2, 0, 0, 0, AllowGhostConversion, 0};
  policy.level[static_cast<uint8_t>(RealizationLevel::P3Transformation)] =
      MutationBudget{3, 0, 0, 0, AllowOptionalAdds, 0};
  return policy;
}

constexpr BarTrajectory kStatementTrajectory = {
    1,
    1,
    {BarFunction::Statement,
     BarFunction::Statement,
     BarFunction::Statement,
     BarFunction::Statement}};

constexpr TrajectoryRef kStatementRef = {
    1,
    100,
    kAllRealizationLevels};

constexpr RhythmRoleMask kDrumsAndBass =
    rhythmRoleBit(RhythmRole::Kick) |
    rhythmRoleBit(RhythmRole::Backbeat) |
    rhythmRoleBit(RhythmRole::ClosedHat) |
    rhythmRoleBit(RhythmRole::OpenHat) |
    rhythmRoleBit(RhythmRole::Percussion) |
    rhythmRoleBit(RhythmRole::BassRhythm);

constexpr RhythmRoleMask kDrumsBassChord =
    static_cast<RhythmRoleMask>(
        kDrumsAndBass | rhythmRoleBit(RhythmRole::ChordRhythm));

constexpr RhythmRoleMask kSwingRhythmRoles =
    rhythmRoleBit(RhythmRole::ClosedHat) |
    rhythmRoleBit(RhythmRole::OpenHat) |
    rhythmRoleBit(RhythmRole::Percussion) |
    rhythmRoleBit(RhythmRole::BassRhythm);

constexpr RhythmRoleMask kShuffleRhythmRoles =
    static_cast<RhythmRoleMask>(
        kSwingRhythmRoles | rhythmRoleBit(RhythmRole::ChordRhythm));

constexpr RhythmArchetype archetype(
    RhythmArchetypeId id,
    RhythmFamily family,
    RhythmRoleMask activeRoles,
    const LaneGrammar* lanes,
    uint8_t laneCount,
    const ProtectedSpace* spaces,
    uint8_t spaceCount,
    const LaneRelationship* relationships,
    uint8_t relationshipCount,
    DensityContract density,
    TimingEligibility timingEligibility) {
  RhythmArchetype value{};
  value.id = id;
  value.family = family;
  value.allowedPhraseBars = phraseBarsBit(1);
  value.activeRoles = activeRoles;
  value.lanes = lanes;
  value.laneCount = laneCount;
  value.protectedSpaces = spaces;
  value.protectedSpaceCount = spaceCount;
  value.relationships = relationships;
  value.relationshipCount = relationshipCount;
  value.trajectories = &kStatementRef;
  value.trajectoryCount = 1;
  value.timing = timingEligibility;
  value.density = density;
  value.mutation = referenceMutationPolicy();
  return value;
}

// 401 straight_drive --------------------------------------------------------
constexpr LaneGrammar kStraightDriveLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), 0, 2, 4),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(6) | stepBit(14), 0, 2),
    lane(RhythmRole::Percussion, 0, 0, 0,
         stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15), 0, 2),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(4) | stepBit(12), stepBit(2) | stepBit(10),
         2, 4, 1, 0, stepBit(0) | stepBit(8)),
};
constexpr LaneRelationship kStraightDriveRelationships[] = {
    hardCoincide(RhythmRole::Kick, RhythmRole::BassRhythm, 2, 4),
    softFillGaps(RhythmRole::Kick, RhythmRole::Percussion, 35),
};

// 402 offbeat_open_hat ------------------------------------------------------
constexpr LaneGrammar kOffbeatOpenHatLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(0) | stepBit(8),
         stepBit(4) | stepBit(12), 0, 2, 4),
    lane(RhythmRole::OpenHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, 0, 4, 4),
    lane(RhythmRole::Percussion, 0, 0, 0,
         stepBit(3) | stepBit(11), 0, 2),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(4) | stepBit(12), stepBit(6) | stepBit(14),
         2, 4, 1, 0, stepBit(0) | stepBit(8)),
};
constexpr LaneRelationship kOffbeatOpenHatRelationships[] = {
    hardCoincide(RhythmRole::Kick, RhythmRole::BassRhythm, 2, 4),
    softFillGaps(RhythmRole::Kick, RhythmRole::OpenHat, 80),
};

// 403 hypnotic_sparse -------------------------------------------------------
constexpr LaneGrammar kHypnoticSparseLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(12),
         stepBit(4), 0, 1, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), 0, 2, 4),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(6) | stepBit(14), 0, 1),
    lane(RhythmRole::Percussion, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 1),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(8), stepBit(6) | stepBit(14),
         1, 3, 1, 0, stepBit(0) | stepBit(8)),
};
constexpr LaneRelationship kHypnoticSparseRelationships[] = {
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 0, 2, 65),
    softFillGaps(RhythmRole::ClosedHat, RhythmRole::Percussion, 30),
};

// 404 broken_techno ---------------------------------------------------------
constexpr LaneGrammar kBrokenTechnoLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0) | stepBit(10),
         stepBit(6), stepBit(14), 2, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11), 2, 5),
    lane(RhythmRole::OpenHat, 0, 0, stepBit(6) | stepBit(14),
         0, 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(3),
         stepBit(7) | stepBit(15), stepBit(1) | stepBit(9), 1, 4),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(10),
         stepBit(6) | stepBit(14), stepBit(3),
         2, 4, 1, stepBit(10)),
};
constexpr ProtectedSpace kBrokenTechnoSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kBrokenTechnoRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Kick, RhythmRole::Percussion, 1, 2, 65),
};

// 405 straight_acid ---------------------------------------------------------
constexpr LaneGrammar kStraightAcidLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, 0, 4, 4),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(3) | stepBit(11), 0, 2),
    lane(RhythmRole::Percussion, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 2),
    lane(RhythmRole::BassRhythm, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         stepBit(3) | stepBit(11), stepBit(7) | stepBit(15),
         4, 8, 2, stepBit(3) | stepBit(11),
         stepBit(0) | stepBit(8)),
};
constexpr LaneRelationship kStraightAcidRelationships[] = {
    hardOffset(RhythmRole::Kick, RhythmRole::ClosedHat, 2, 2),
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 0, 3, 80),
};

// 406 rolling_acid ----------------------------------------------------------
constexpr LaneGrammar kRollingAcidLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), 0, 2, 4),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(3) | stepBit(11), 0, 2),
    lane(RhythmRole::Percussion, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 2),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(3) | stepBit(5) | stepBit(7) |
             stepBit(11) | stepBit(13) | stepBit(15),
         stepBit(1) | stepBit(9), 2, 10, 2,
         stepBit(3) | stepBit(11), stepBit(0) | stepBit(8)),
};
constexpr LaneRelationship kRollingAcidRelationships[] = {
    hardOffset(RhythmRole::Kick, RhythmRole::ClosedHat, 2, 2),
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 1, 3, 80),
};

// 407 syncopated_acid -------------------------------------------------------
constexpr LaneGrammar kSyncopatedAcidLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0) | stepBit(8),
         stepBit(10), stepBit(6) | stepBit(14), 2, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, stepBit(3) | stepBit(11), 4, 6),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 2),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(7), stepBit(3) | stepBit(15), 0, 3),
    lane(RhythmRole::BassRhythm, 0,
         stepBit(0) | stepBit(3) | stepBit(10),
         stepBit(5) | stepBit(7) | stepBit(13),
         stepBit(1) | stepBit(9) | stepBit(15),
         3, 8, 2, stepBit(3) | stepBit(13), stepBit(0)),
};
constexpr ProtectedSpace kSyncopatedAcidSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick)),
};
constexpr LaneRelationship kSyncopatedAcidRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 1, 3, 85),
};

// 408 sparse_acid -----------------------------------------------------------
constexpr LaneGrammar kSparseAcidLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0) | stepBit(8),
         stepBit(12), 0, 2, 3),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         0, stepBit(6) | stepBit(14), 2, 4),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 1),
    lane(RhythmRole::Percussion, 0, 0, 0,
         stepBit(3) | stepBit(11), 0, 1),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11),
         1, 4, 1, stepBit(6), stepBit(0) | stepBit(14)),
};
constexpr LaneRelationship kSparseAcidRelationships[] = {
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 1, 3, 75),
    softFillGaps(RhythmRole::ClosedHat, RhythmRole::BassRhythm, 45),
};

// 409 one_drop_space --------------------------------------------------------
constexpr LaneGrammar kOneDropSpaceLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(8), 0, 0, 1, 1),
    lane(RhythmRole::Backbeat, 0, stepBit(8), 0, 0, 1, 1),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(6) |
         stepBit(10) | stepBit(14), 0, 0, 4, 4),
    lane(RhythmRole::OpenHat, 0, 0, stepBit(3) | stepBit(11),
         0, 0, 2),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(7) | stepBit(15), stepBit(5) | stepBit(13), 0, 3),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(6) | stepBit(14), stepBit(10),
         1, 4, 1, 0, stepBit(0) | stepBit(14)),
    lane(RhythmRole::ChordRhythm, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, 0, 4, 4, 0, stepBit(2) | stepBit(10)),
};
constexpr ProtectedSpace kOneDropSpaceSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kOneDropSpaceRelationships[] = {
    hardCoincide(RhythmRole::Kick, RhythmRole::Backbeat, 1, 1),
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 1, 6, 65),
};

// 410 steppers --------------------------------------------------------------
constexpr LaneGrammar kSteppersLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, 0, 4, 4),
    lane(RhythmRole::OpenHat, 0, 0,
         stepBit(3) | stepBit(11), 0, 0, 2),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(7) | stepBit(15), stepBit(5) | stepBit(13), 0, 3),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(6) | stepBit(14), stepBit(10),
         2, 4, 1, 0, stepBit(0) | stepBit(14)),
    lane(RhythmRole::ChordRhythm, 0,
         stepBit(2) | stepBit(10), stepBit(6) | stepBit(14),
         0, 2, 4, 0, stepBit(2) | stepBit(10)),
};
constexpr LaneRelationship kSteppersRelationships[] = {
    softRespond(RhythmRole::Kick, RhythmRole::ChordRhythm, 2, 2, 75),
    softFillGaps(RhythmRole::Kick, RhythmRole::Percussion, 35),
};

// 411 sparse_skank ----------------------------------------------------------
constexpr LaneGrammar kSparseSkankLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0), stepBit(10),
         stepBit(14), 1, 3),
    lane(RhythmRole::Backbeat, 0, stepBit(8), 0, 0, 1, 1),
    lane(RhythmRole::ClosedHat, 0, stepBit(6) | stepBit(14),
         0, stepBit(2) | stepBit(10), 2, 4),
    lane(RhythmRole::OpenHat, 0, 0,
         stepBit(3) | stepBit(11), 0, 0, 2),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(7) | stepBit(15), stepBit(5) | stepBit(13), 1, 4),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(6) | stepBit(14), stepBit(10),
         1, 4, 1, 0, stepBit(0) | stepBit(14)),
    lane(RhythmRole::ChordRhythm, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), 0,
         2, 4, 0, 0, stepBit(2) | stepBit(10)),
};
constexpr ProtectedSpace kSparseSkankSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm) |
                       rhythmRoleBit(RhythmRole::ChordRhythm)),
};
constexpr LaneRelationship kSparseSkankRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Kick, RhythmRole::ChordRhythm, 2, 3, 65),
};

// 412 chord_response --------------------------------------------------------
constexpr LaneGrammar kChordResponseLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0),
         stepBit(10), stepBit(14), 1, 3),
    lane(RhythmRole::Backbeat, 0, stepBit(8), 0, 0, 1, 1),
    lane(RhythmRole::ClosedHat, 0, stepBit(6) | stepBit(14),
         stepBit(2) | stepBit(10), 0, 2, 4),
    lane(RhythmRole::OpenHat, 0, 0, stepBit(3) | stepBit(11),
         0, 0, 2),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(7) | stepBit(15), stepBit(5) | stepBit(13), 0, 3),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(8) | stepBit(14), stepBit(6),
         1, 4, 1, 0, stepBit(0) | stepBit(14)),
    lane(RhythmRole::ChordRhythm, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11),
         2, 5, 1, stepBit(3) | stepBit(11),
         stepBit(2) | stepBit(10)),
};
constexpr ProtectedSpace kChordResponseSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kChordResponseRelationships[] = {
    softRespond(RhythmRole::Kick, RhythmRole::ChordRhythm, 2, 3, 85),
    softFillGaps(RhythmRole::ChordRhythm, RhythmRole::Percussion, 40),
};

// 413 two_step_roll ---------------------------------------------------------
constexpr LaneGrammar kTwoStepRollLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0) | stepBit(10),
         stepBit(6) | stepBit(14), stepBit(3), 2, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, stepBit(3) | stepBit(7), 4, 6),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(3) | stepBit(11),
         stepBit(7) | stepBit(15), stepBit(1) | stepBit(9), 2, 6),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11),
         2, 5, 1, stepBit(6) | stepBit(14)),
};
constexpr ProtectedSpace kTwoStepRollSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kTwoStepRollRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Kick, RhythmRole::Percussion, 1, 2, 70),
};

// 414 ghosted_roll ----------------------------------------------------------
constexpr LaneGrammar kGhostedRollLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0) | stepBit(10),
         stepBit(6) | stepBit(14), 0, 2, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, stepBit(1) | stepBit(3) | stepBit(5) | stepBit(7) |
             stepBit(9) | stepBit(11) | stepBit(13) | stepBit(15),
         4, 8, 3),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(3) | stepBit(11),
         stepBit(7) | stepBit(15), stepBit(1) | stepBit(9), 2, 6, 2),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11),
         2, 5, 1, stepBit(6) | stepBit(14)),
};
constexpr ProtectedSpace kGhostedRollSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kGhostedRollRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softFillGaps(RhythmRole::Kick, RhythmRole::ClosedHat, 65),
};

// 415 sparse_fast_break -----------------------------------------------------
constexpr LaneGrammar kSparseFastBreakLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0),
         stepBit(10), stepBit(6) | stepBit(14), 1, 3),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), 0, 2, 4),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 1),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(3) | stepBit(11), stepBit(7) | stepBit(15), 1, 4),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(6) | stepBit(14), stepBit(10),
         1, 4, 1, stepBit(6)),
};
constexpr ProtectedSpace kSparseFastBreakSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick)),
};
constexpr LaneRelationship kSparseFastBreakRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Kick, RhythmRole::Percussion, 1, 3, 60),
};

// 416 halftime_switch -------------------------------------------------------
constexpr LaneGrammar kHalftimeSwitchLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0) | stepBit(10),
         stepBit(14), 0, 2, 3),
    lane(RhythmRole::Backbeat, 0, stepBit(8),
         0, 0, 1, 1),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, stepBit(3) | stepBit(11), 4, 6),
    lane(RhythmRole::OpenHat, 0, 0, 0,
         stepBit(7) | stepBit(15), 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(4) | stepBit(12),
         0, stepBit(3) | stepBit(11), 2, 4),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(10) | stepBit(14), 0,
         2, 4, 1, 0, stepBit(0) | stepBit(8)),
};
constexpr ProtectedSpace kHalftimeSwitchSpaces[] = {
    protectedSpace(stepBit(8), rhythmRoleBit(RhythmRole::Kick)),
};
constexpr LaneRelationship kHalftimeSwitchRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Backbeat, RhythmRole::Percussion, 3, 4, 55),
};

// 417 classic_2step ---------------------------------------------------------
constexpr LaneGrammar kClassicTwoStepLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0),
         stepBit(6) | stepBit(10), stepBit(14), 2, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11), 2, 6),
    lane(RhythmRole::OpenHat, 0, 0,
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11), 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(3),
         stepBit(7) | stepBit(15), stepBit(1) | stepBit(9), 2, 5),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(3) | stepBit(10), stepBit(6) | stepBit(14),
         2, 5, 1, 0, stepBit(0) | stepBit(10)),
};
constexpr ProtectedSpace kClassicTwoStepSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kClassicTwoStepRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softFillGaps(RhythmRole::Kick, RhythmRole::BassRhythm, 70),
};

// 418 skippy_2step ----------------------------------------------------------
constexpr LaneGrammar kSkippyTwoStepLanes[] = {
    lane(RhythmRole::Kick, 0, stepBit(0),
         stepBit(3) | stepBit(10), stepBit(6) | stepBit(14), 2, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15), 4, 7),
    lane(RhythmRole::OpenHat, 0, 0,
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11), 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(3),
         stepBit(7) | stepBit(15), stepBit(1) | stepBit(9), 1, 5),
    lane(RhythmRole::BassRhythm, 0, stepBit(0),
         stepBit(3) | stepBit(10), stepBit(6) | stepBit(14),
         2, 5, 1, stepBit(3)),
};
constexpr ProtectedSpace kSkippyTwoStepSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kSkippyTwoStepRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softFillGaps(RhythmRole::Kick, RhythmRole::BassRhythm, 80),
};

// 419 shuffled_4x4 ----------------------------------------------------------
constexpr LaneGrammar kShuffledFourFourLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0, 0, 4, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0, stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11), 2, 5),
    lane(RhythmRole::OpenHat, 0, stepBit(6) | stepBit(14),
         0, stepBit(3) | stepBit(11), 2, 4),
    lane(RhythmRole::Percussion, 0, 0,
         stepBit(7) | stepBit(15), stepBit(3) | stepBit(11), 0, 3),
    lane(RhythmRole::BassRhythm, 0, stepBit(0) | stepBit(8),
         stepBit(6) | stepBit(14), stepBit(3) | stepBit(11),
         2, 5, 1, stepBit(6) | stepBit(14)),
};
constexpr LaneRelationship kShuffledFourFourRelationships[] = {
    hardCoincide(RhythmRole::Kick, RhythmRole::BassRhythm, 2, 4),
    softFillGaps(RhythmRole::Kick, RhythmRole::ClosedHat, 70),
};

// 420 machine_syncopation ---------------------------------------------------
constexpr LaneGrammar kMachineSyncopationLanes[] = {
    lane(RhythmRole::Kick, 0,
         stepBit(0) | stepBit(7) | stepBit(10),
         stepBit(14), 0, 3, 4),
    lane(RhythmRole::Backbeat, 0, stepBit(4) | stepBit(12),
         0, 0, 2, 2),
    lane(RhythmRole::ClosedHat, 0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0, stepBit(3) | stepBit(11), 4, 6),
    lane(RhythmRole::OpenHat, 0, 0,
         stepBit(7) | stepBit(15), 0, 0, 2),
    lane(RhythmRole::Percussion, 0, stepBit(3) | stepBit(11),
         stepBit(7) | stepBit(15), stepBit(1) | stepBit(9), 2, 6),
    lane(RhythmRole::BassRhythm, 0,
         stepBit(0) | stepBit(7),
         stepBit(10) | stepBit(14), stepBit(3) | stepBit(11),
         2, 5, 1, stepBit(7)),
};
constexpr ProtectedSpace kMachineSyncopationSpaces[] = {
    protectedSpace(stepBit(4) | stepBit(12),
                   rhythmRoleBit(RhythmRole::Kick) |
                       rhythmRoleBit(RhythmRole::BassRhythm)),
};
constexpr LaneRelationship kMachineSyncopationRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
    softRespond(RhythmRole::Kick, RhythmRole::Percussion, 1, 2, 75),
};

constexpr RhythmArchetype kArchetypes[] = {
    archetype(401, RhythmFamily::FourFloor, kDrumsAndBass,
              kStraightDriveLanes, 6, nullptr, 0,
              kStraightDriveRelationships, 2,
              DensityContract{10, 12, 18, 4},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(402, RhythmFamily::FourFloor, kDrumsAndBass,
              kOffbeatOpenHatLanes, 6, nullptr, 0,
              kOffbeatOpenHatRelationships, 2,
              DensityContract{14, 16, 20, 3},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(403, RhythmFamily::FourFloor, kDrumsAndBass,
              kHypnoticSparseLanes, 6, nullptr, 0,
              kHypnoticSparseRelationships, 2,
              DensityContract{10, 11, 16, 3},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kSwingRhythmRoles)),
    archetype(404, RhythmFamily::MachineSyncopation, kDrumsAndBass,
              kBrokenTechnoLanes, 6,
              kBrokenTechnoSpaces, 1,
              kBrokenTechnoRelationships, 2,
              DensityContract{10, 13, 23, 6},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(405, RhythmFamily::FourFloor, kDrumsAndBass,
              kStraightAcidLanes, 6, nullptr, 0,
              kStraightAcidRelationships, 2,
              DensityContract{14, 18, 24, 5},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kSwingRhythmRoles)),
    archetype(406, RhythmFamily::FourFloor, kDrumsAndBass,
              kRollingAcidLanes, 6, nullptr, 0,
              kRollingAcidRelationships, 2,
              DensityContract{10, 14, 24, 6},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kSwingRhythmRoles)),
    archetype(407, RhythmFamily::MachineSyncopation, kDrumsAndBass,
              kSyncopatedAcidLanes, 6,
              kSyncopatedAcidSpaces, 1,
              kSyncopatedAcidRelationships, 2,
              DensityContract{11, 15, 27, 7},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kSwingRhythmRoles)),
    archetype(408, RhythmFamily::SparsePulse, kDrumsAndBass,
              kSparseAcidLanes, 6, nullptr, 0,
              kSparseAcidRelationships, 2,
              DensityContract{8, 10, 18, 4},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kSwingRhythmRoles)),
    archetype(409, RhythmFamily::DubPulse, kDrumsBassChord,
              kOneDropSpaceLanes, 7,
              kOneDropSpaceSpaces, 1,
              kOneDropSpaceRelationships, 2,
              DensityContract{13, 15, 21, 4},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kShuffleRhythmRoles)),
    archetype(410, RhythmFamily::DubPulse, kDrumsBassChord,
              kSteppersLanes, 7, nullptr, 0,
              kSteppersRelationships, 2,
              DensityContract{16, 19, 25, 5},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kShuffleRhythmRoles)),
    archetype(411, RhythmFamily::DubPulse, kDrumsBassChord,
              kSparseSkankLanes, 7,
              kSparseSkankSpaces, 1,
              kSparseSkankRelationships, 2,
              DensityContract{8, 10, 22, 5},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kShuffleRhythmRoles)),
    archetype(412, RhythmFamily::DubPulse, kDrumsBassChord,
              kChordResponseLanes, 7,
              kChordResponseSpaces, 1,
              kChordResponseRelationships, 2,
              DensityContract{9, 12, 24, 6},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     kShuffleRhythmRoles)),
    archetype(413, RhythmFamily::Breakbeat, kDrumsAndBass,
              kTwoStepRollLanes, 6,
              kTwoStepRollSpaces, 1,
              kTwoStepRollRelationships, 2,
              DensityContract{12, 15, 25, 6},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(414, RhythmFamily::Breakbeat, kDrumsAndBass,
              kGhostedRollLanes, 6,
              kGhostedRollSpaces, 1,
              kGhostedRollRelationships, 2,
              DensityContract{12, 17, 30, 10},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(415, RhythmFamily::Breakbeat, kDrumsAndBass,
              kSparseFastBreakLanes, 6,
              kSparseFastBreakSpaces, 1,
              kSparseFastBreakRelationships, 2,
              DensityContract{8, 11, 19, 5},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(416, RhythmFamily::Breakbeat, kDrumsAndBass,
              kHalftimeSwitchLanes, 6,
              kHalftimeSwitchSpaces, 1,
              kHalftimeSwitchRelationships, 2,
              DensityContract{13, 15, 23, 5},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(417, RhythmFamily::UkTwoStep, kDrumsAndBass,
              kClassicTwoStepLanes, 6,
              kClassicTwoStepSpaces, 1,
              kClassicTwoStepRelationships, 2,
              DensityContract{10, 13, 24, 6},
              timing(TimingCompatibility::ShufflePreferred,
                     stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
                     kSwingRhythmRoles)),
    archetype(418, RhythmFamily::UkTwoStep, kDrumsAndBass,
              kSkippyTwoStepLanes, 6,
              kSkippyTwoStepSpaces, 1,
              kSkippyTwoStepRelationships, 2,
              DensityContract{11, 15, 27, 8},
              timing(TimingCompatibility::ShufflePreferred,
                     stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
                     kSwingRhythmRoles)),
    archetype(419, RhythmFamily::UkTwoStep, kDrumsAndBass,
              kShuffledFourFourLanes, 6, nullptr, 0,
              kShuffledFourFourRelationships, 2,
              DensityContract{14, 17, 25, 6},
              timing(TimingCompatibility::ShufflePreferred,
                     stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
                     kSwingRhythmRoles)),
    archetype(420, RhythmFamily::MachineSyncopation, kDrumsAndBass,
              kMachineSyncopationLanes, 6,
              kMachineSyncopationSpaces, 1,
              kMachineSyncopationRelationships, 2,
              DensityContract{13, 16, 27, 7},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
};

constexpr Definition kDefinitions[] = {
    {Archetype::StraightDrive, 401, "straight_drive", RhythmFamily::FourFloor, 122, 136},
    {Archetype::OffbeatOpenHat, 402, "offbeat_open_hat", RhythmFamily::FourFloor, 120, 134},
    {Archetype::HypnoticSparse, 403, "hypnotic_sparse", RhythmFamily::FourFloor, 124, 140},
    {Archetype::BrokenTechno, 404, "broken_techno", RhythmFamily::MachineSyncopation, 124, 142},
    {Archetype::StraightAcid, 405, "straight_acid", RhythmFamily::FourFloor, 120, 134},
    {Archetype::RollingAcid, 406, "rolling_acid", RhythmFamily::FourFloor, 122, 138},
    {Archetype::SyncopatedAcid, 407, "syncopated_acid", RhythmFamily::MachineSyncopation, 122, 140},
    {Archetype::SparseAcid, 408, "sparse_acid", RhythmFamily::SparsePulse, 116, 132},
    {Archetype::OneDropSpace, 409, "one_drop_space", RhythmFamily::DubPulse, 72, 122},
    {Archetype::Steppers, 410, "steppers", RhythmFamily::DubPulse, 108, 132},
    {Archetype::SparseSkank, 411, "sparse_skank", RhythmFamily::DubPulse, 88, 124},
    {Archetype::ChordResponse, 412, "chord_response", RhythmFamily::DubPulse, 96, 128},
    {Archetype::TwoStepRoll, 413, "two_step_roll", RhythmFamily::Breakbeat, 160, 180},
    {Archetype::GhostedRoll, 414, "ghosted_roll", RhythmFamily::Breakbeat, 164, 182},
    {Archetype::SparseFastBreak, 415, "sparse_fast_break", RhythmFamily::Breakbeat, 158, 178},
    {Archetype::HalftimeSwitch, 416, "halftime_switch", RhythmFamily::Breakbeat, 150, 176},
    {Archetype::ClassicTwoStep, 417, "classic_2step", RhythmFamily::UkTwoStep, 128, 138},
    {Archetype::SkippyTwoStep, 418, "skippy_2step", RhythmFamily::UkTwoStep, 130, 142},
    {Archetype::ShuffledFourFour, 419, "shuffled_4x4", RhythmFamily::UkTwoStep, 126, 140},
    {Archetype::MachineSyncopation, 420, "machine_syncopation", RhythmFamily::MachineSyncopation, 116, 142},
};

constexpr RhythmCatalogView kCatalog = {
    kArchetypes,
    static_cast<uint16_t>(sizeof(kArchetypes) / sizeof(kArchetypes[0])),
    &kStatementTrajectory,
    1};

static_assert(sizeof(kArchetypes) / sizeof(kArchetypes[0]) == 20,
              "Stage 3 reference package must contain exactly 20 archetypes");
static_assert(sizeof(kDefinitions) / sizeof(kDefinitions[0]) ==
                  static_cast<uint8_t>(Archetype::Count),
              "Stage 3 definition table is incomplete");

}  // namespace

const RhythmCatalogView& catalog() {
  return kCatalog;
}

uint8_t definitionCount() {
  return static_cast<uint8_t>(sizeof(kDefinitions) / sizeof(kDefinitions[0]));
}

const Definition& definition(uint8_t index) {
  if (index >= definitionCount()) index = 0;
  return kDefinitions[index];
}

const Definition* definitionFor(Archetype key) {
  for (uint8_t i = 0; i < definitionCount(); ++i) {
    if (kDefinitions[i].key == key) return &kDefinitions[i];
  }
  return nullptr;
}

const RhythmArchetype* archetypeFor(Archetype key) {
  const Definition* def = definitionFor(key);
  if (!def) return nullptr;
  for (uint16_t i = 0; i < kCatalog.archetypeCount; ++i) {
    if (kCatalog.archetypes[i].id == def->archetypeId) {
      return &kCatalog.archetypes[i];
    }
  }
  return nullptr;
}

}  // namespace ReferenceVocabulary
}  // namespace GroovePuterRhythm
