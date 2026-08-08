#include "rhythm_audition_catalog.h"

namespace GroovePuterRhythm {
namespace Audition {
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

constexpr MutationPolicy auditionMutationPolicy() {
  MutationPolicy policy{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P1Canonical)] =
      MutationBudget{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)] =
      MutationBudget{2, 0, 0, 0, AllowGhostConversion, 0};
  policy.level[static_cast<uint8_t>(RealizationLevel::P3Transformation)] =
      MutationBudget{3, 0, 0, 0, AllowOptionalAdds, 0};
  return policy;
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

constexpr ProtectedSpace protectedSpace(StepMask steps,
                                         RhythmRoleMask roles) {
  ProtectedSpace value{};
  value.steps = steps;
  value.affectedRoles = roles;
  return value;
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

// --- straight_drive -------------------------------------------------------
constexpr LaneGrammar kStraightDriveLanes[] = {
    lane(RhythmRole::Kick,
         0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0,
         0,
         4,
         4),
    lane(RhythmRole::Backbeat,
         0,
         stepBit(4) | stepBit(12),
         0,
         0,
         2,
         2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14),
         0,
         2,
         4),
    lane(RhythmRole::OpenHat,
         0,
         0,
         0,
         stepBit(6) | stepBit(14),
         0,
         2),
    lane(RhythmRole::Percussion,
         0,
         0,
         0,
         stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
         0,
         2),
    lane(RhythmRole::BassRhythm,
         0,
         stepBit(0) | stepBit(8),
         stepBit(4) | stepBit(12),
         stepBit(2) | stepBit(10),
         2,
         4,
         1,
         0,
         stepBit(0) | stepBit(8)),
};

constexpr LaneRelationship kStraightDriveRelationships[] = {
    hardCoincide(RhythmRole::Kick, RhythmRole::BassRhythm, 2, 4),
    softFillGaps(RhythmRole::Kick, RhythmRole::Percussion, 35),
};

// --- rolling_acid ---------------------------------------------------------
constexpr LaneGrammar kRollingAcidLanes[] = {
    lane(RhythmRole::Kick,
         0,
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         0,
         0,
         4,
         4),
    lane(RhythmRole::Backbeat,
         0,
         stepBit(4) | stepBit(12),
         0,
         0,
         2,
         2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14),
         0,
         2,
         4),
    lane(RhythmRole::OpenHat,
         0,
         0,
         0,
         stepBit(3) | stepBit(11),
         0,
         2),
    lane(RhythmRole::Percussion,
         0,
         0,
         0,
         stepBit(7) | stepBit(15),
         0,
         2),
    lane(RhythmRole::BassRhythm,
         0,
         stepBit(0) | stepBit(8),
         stepBit(3) | stepBit(5) | stepBit(7) |
             stepBit(11) | stepBit(13) | stepBit(15),
         stepBit(1) | stepBit(9),
         4,
         10,
         2,
         stepBit(3) | stepBit(11),
         stepBit(0) | stepBit(8)),
};

constexpr LaneRelationship kRollingAcidRelationships[] = {
    hardOffset(RhythmRole::Kick, RhythmRole::ClosedHat, 2, 2),
    softRespond(RhythmRole::Kick, RhythmRole::BassRhythm, 1, 3, 80),
};

// --- classic_2step --------------------------------------------------------
constexpr LaneGrammar kClassicTwoStepLanes[] = {
    lane(RhythmRole::Kick,
         0,
         stepBit(0),
         stepBit(6) | stepBit(10),
         stepBit(14),
         2,
         4),
    lane(RhythmRole::Backbeat,
         0,
         stepBit(4) | stepBit(12),
         0,
         0,
         2,
         2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14),
         stepBit(3) | stepBit(11),
         2,
         6),
    lane(RhythmRole::OpenHat,
         0,
         0,
         stepBit(6) | stepBit(14),
         stepBit(3) | stepBit(11),
         0,
         2),
    lane(RhythmRole::Percussion,
         0,
         stepBit(3),
         stepBit(7) | stepBit(15),
         stepBit(1) | stepBit(9),
         2,
         5),
    lane(RhythmRole::BassRhythm,
         0,
         stepBit(0),
         stepBit(3) | stepBit(10),
         stepBit(6) | stepBit(14),
         2,
         5,
         1,
         0,
         stepBit(0) | stepBit(10)),
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

// --- two_step_roll --------------------------------------------------------
constexpr LaneGrammar kTwoStepRollLanes[] = {
    lane(RhythmRole::Kick,
         0,
         stepBit(0) | stepBit(10),
         stepBit(6) | stepBit(14),
         stepBit(3),
         2,
         4),
    lane(RhythmRole::Backbeat,
         0,
         stepBit(4) | stepBit(12),
         0,
         0,
         2,
         2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0,
         stepBit(3) | stepBit(7),
         4,
         6),
    lane(RhythmRole::OpenHat,
         0,
         0,
         0,
         stepBit(7) | stepBit(15),
         0,
         2),
    lane(RhythmRole::Percussion,
         0,
         stepBit(3) | stepBit(11),
         stepBit(7) | stepBit(15),
         stepBit(1) | stepBit(9),
         2,
         6),
    lane(RhythmRole::BassRhythm,
         0,
         stepBit(0) | stepBit(8),
         stepBit(6) | stepBit(14),
         stepBit(3) | stepBit(11),
         2,
         5,
         1,
         stepBit(6) | stepBit(14)),
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

// --- sparse_skank ---------------------------------------------------------
constexpr LaneGrammar kSparseSkankLanes[] = {
    lane(RhythmRole::Kick,
         0,
         stepBit(0),
         stepBit(10),
         stepBit(14),
         1,
         3),
    lane(RhythmRole::Backbeat,
         0,
         stepBit(8),
         0,
         0,
         1,
         1),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(6) | stepBit(14),
         0,
         stepBit(2) | stepBit(10),
         2,
         4),
    lane(RhythmRole::OpenHat,
         0,
         0,
         stepBit(3) | stepBit(11),
         0,
         0,
         2),
    lane(RhythmRole::Percussion,
         0,
         0,
         stepBit(7) | stepBit(15),
         stepBit(5) | stepBit(13),
         1,
         4),
    lane(RhythmRole::BassRhythm,
         0,
         stepBit(0),
         stepBit(6) | stepBit(14),
         stepBit(10),
         1,
         4,
         1,
         0,
         stepBit(0) | stepBit(14)),
    lane(RhythmRole::ChordRhythm,
         0,
         stepBit(2) | stepBit(10),
         stepBit(6) | stepBit(14),
         0,
         2,
         4,
         0,
         0,
         stepBit(2) | stepBit(10)),
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
  value.mutation = auditionMutationPolicy();
  return value;
}

constexpr RhythmRoleMask kDrumsAndBass =
    rhythmRoleBit(RhythmRole::Kick) |
    rhythmRoleBit(RhythmRole::Backbeat) |
    rhythmRoleBit(RhythmRole::ClosedHat) |
    rhythmRoleBit(RhythmRole::OpenHat) |
    rhythmRoleBit(RhythmRole::Percussion) |
    rhythmRoleBit(RhythmRole::BassRhythm);

constexpr RhythmArchetype kArchetypes[] = {
    archetype(301,
              RhythmFamily::FourFloor,
              kDrumsAndBass,
              kStraightDriveLanes,
              6,
              nullptr,
              0,
              kStraightDriveRelationships,
              2,
              DensityContract{10, 12, 18, 4},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(302,
              RhythmFamily::FourFloor,
              kDrumsAndBass,
              kRollingAcidLanes,
              6,
              nullptr,
              0,
              kRollingAcidRelationships,
              2,
              DensityContract{14, 16, 24, 6},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     rhythmRoleBit(RhythmRole::BassRhythm) |
                         rhythmRoleBit(RhythmRole::ClosedHat))),
    archetype(303,
              RhythmFamily::UkTwoStep,
              kDrumsAndBass,
              kClassicTwoStepLanes,
              6,
              kClassicTwoStepSpaces,
              1,
              kClassicTwoStepRelationships,
              2,
              DensityContract{10, 13, 24, 6},
              timing(TimingCompatibility::ShufflePreferred,
                     stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
                     rhythmRoleBit(RhythmRole::ClosedHat) |
                         rhythmRoleBit(RhythmRole::BassRhythm))),
    archetype(304,
              RhythmFamily::Breakbeat,
              kDrumsAndBass,
              kTwoStepRollLanes,
              6,
              kTwoStepRollSpaces,
              1,
              kTwoStepRollRelationships,
              2,
              DensityContract{12, 15, 25, 6},
              timing(TimingCompatibility::StraightOnly, 0, 0)),
    archetype(305,
              RhythmFamily::DubPulse,
              static_cast<RhythmRoleMask>(
                  kDrumsAndBass | rhythmRoleBit(RhythmRole::ChordRhythm)),
              kSparseSkankLanes,
              7,
              kSparseSkankSpaces,
              1,
              kSparseSkankRelationships,
              2,
              DensityContract{8, 10, 22, 5},
              timing(TimingCompatibility::SwingCompatible,
                     stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                     rhythmRoleBit(RhythmRole::ClosedHat) |
                         rhythmRoleBit(RhythmRole::ChordRhythm))),
};

constexpr Definition kDefinitions[] = {
    {Archetype::StraightDrive, 301, "straight_drive", 128},
    {Archetype::RollingAcid, 302, "rolling_acid", 126},
    {Archetype::ClassicTwoStep, 303, "classic_2step", 132},
    {Archetype::TwoStepRoll, 304, "two_step_roll", 174},
    {Archetype::SparseSkank, 305, "sparse_skank", 118},
};

constexpr RhythmCatalogView kCatalog = {
    kArchetypes,
    static_cast<uint16_t>(sizeof(kArchetypes) / sizeof(kArchetypes[0])),
    &kStatementTrajectory,
    1};

static_assert(sizeof(kDefinitions) / sizeof(kDefinitions[0]) ==
                  static_cast<uint8_t>(Archetype::Count),
              "Stage 3A definition table is incomplete");

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

}  // namespace Audition
}  // namespace GroovePuterRhythm
