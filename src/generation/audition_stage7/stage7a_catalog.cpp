#include "stage7a_catalog.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {
namespace {

constexpr LaneGrammar lane(RhythmRole role,
                           StepMask canonical,
                           StepMask preferred,
                           StepMask optional,
                           uint8_t structuralMin,
                           uint8_t structuralMax,
                           uint8_t ornamentMax = 1) {
  LaneGrammar value{};
  value.role = role;
  value.canonicalAnchors = canonical;
  value.preferred = preferred;
  value.optional = optional;
  value.structuralMin = structuralMin;
  value.structuralMax = structuralMax;
  value.ornamentMax = ornamentMax;
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

constexpr TimingEligibility straightTiming() {
  TimingEligibility timing{};
  timing.compatibility = TimingCompatibility::StraightOnly;
  return timing;
}

constexpr TimingEligibility swingCompatible(StepMask sensitive,
                                             RhythmRoleMask roles) {
  TimingEligibility timing{};
  timing.compatibility = TimingCompatibility::SwingCompatible;
  timing.sensitiveSteps = sensitive;
  timing.affectedRoles = roles;
  return timing;
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

constexpr RhythmArchetype archetype(
    RhythmArchetypeId id,
    RhythmFamily family,
    RhythmRoleMask activeRoles,
    const LaneGrammar* lanes,
    uint8_t laneCount,
    const LaneRelationship* relationships,
    uint8_t relationshipCount,
    DensityContract density,
    TimingEligibility timing) {
  RhythmArchetype value{};
  value.id = id;
  value.family = family;
  value.allowedPhraseBars = phraseBarsBit(1);
  value.activeRoles = activeRoles;
  value.lanes = lanes;
  value.laneCount = laneCount;
  value.relationships = relationships;
  value.relationshipCount = relationshipCount;
  value.trajectories = &kStatementRef;
  value.trajectoryCount = 1;
  value.timing = timing;
  value.density = density;
  value.mutation = auditionMutationPolicy();
  return value;
}

constexpr RhythmRoleMask kKickBackbeatHatsPerc =
    rhythmRoleBit(RhythmRole::Kick) |
    rhythmRoleBit(RhythmRole::Backbeat) |
    rhythmRoleBit(RhythmRole::ClosedHat) |
    rhythmRoleBit(RhythmRole::OpenHat) |
    rhythmRoleBit(RhythmRole::Percussion);

constexpr RhythmRoleMask kKickPerc =
    rhythmRoleBit(RhythmRole::Kick) |
    rhythmRoleBit(RhythmRole::Percussion);

constexpr RhythmRoleMask kKickBackbeatHats =
    rhythmRoleBit(RhythmRole::Kick) |
    rhythmRoleBit(RhythmRole::Backbeat) |
    rhythmRoleBit(RhythmRole::ClosedHat) |
    rhythmRoleBit(RhythmRole::OpenHat);

// HARD_02: multi-provenance machine/electro cluster. The source-common full
// skeleton is deliberately NOT copied. Only a smaller identity core is kept;
// the remaining recurring coordinates become legal weighted variation.
constexpr LaneGrammar kStaggeredMachineLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(10),
         stepBit(6) | stepBit(13),
         0,
         2, 4),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12) |
             stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
         2, 6, 2),
    lane(RhythmRole::OpenHat,
         0, 0,
         stepBit(2) | stepBit(10) | stepBit(14),
         0, 2),
    lane(RhythmRole::Percussion,
         0,
         stepBit(3) | stepBit(11),
         stepBit(1) | stepBit(5) | stepBit(7) |
             stepBit(9) | stepBit(13) | stepBit(15),
         0, 4, 2),
};
constexpr LaneRelationship kStaggeredMachineRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

// HARD_05: multi-provenance Afro/Bossa cluster. This is intentionally a
// mapping-sensitive listening hypothesis; ambiguous source cymbal material is
// not guessed into either hat lane. The dense recurring cycle is generalized
// into a smaller kick core plus preferred/optional motion.
constexpr LaneGrammar kCrossCycleLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(8),
         stepBit(3) | stepBit(4) | stepBit(11) | stepBit(12),
         stepBit(7) | stepBit(15),
         4, 8),
    lane(RhythmRole::Percussion,
         stepBit(0),
         stepBit(3) | stepBit(6) | stepBit(10) | stepBit(12),
         stepBit(7) | stepBit(9) | stepBit(13),
         2, 5, 2),
};
constexpr LaneRelationship kCrossCycleRelationships[] = {
    hardCoincide(RhythmRole::Kick, RhythmRole::Percussion, 1, 5),
};

// HARD_04: single-root breaks/DnB/funk challenger. Kick/backbeat carry the
// identity while hats remain a broad but bounded secondary mesh.
constexpr LaneGrammar kBreakHalfstepLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0),
         stepBit(10),
         0,
         1, 2),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
         2, 6, 2),
    lane(RhythmRole::OpenHat,
         0, 0, stepBit(14),
         0, 1),
};
constexpr LaneRelationship kBreakHalfstepRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

// HARD_09: compact rock challenger. The exact observed skeleton is softened so
// the audition tests a reusable push grammar rather than replaying a source.
constexpr LaneGrammar kRockPushLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(8),
         stepBit(7) | stepBit(10),
         0,
         2, 4),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(0) | stepBit(2) | stepBit(4) | stepBit(6) |
             stepBit(8) | stepBit(10) | stepBit(12) | stepBit(14),
         0,
         4, 8, 1),
    lane(RhythmRole::OpenHat,
         0, 0, stepBit(14),
         0, 1),
};
constexpr LaneRelationship kRockPushRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

// HARD_03: intentionally conservative halftime/basic control. If this sounds
// indistinguishable from an existing runtime grammar, that is a successful
// falsification result rather than a failure of the harness.
constexpr LaneGrammar kHalfbackControlLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0),
         stepBit(8),
         0,
         1, 2),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         0,
         0, 4, 1),
    lane(RhythmRole::OpenHat,
         0, 0,
         stepBit(6) | stepBit(14),
         0, 2),
    lane(RhythmRole::Percussion,
         0, 0,
         stepBit(4) | stepBit(12),
         0, 2),
};
constexpr LaneRelationship kHalfbackControlRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

constexpr RhythmArchetype kArchetypes[] = {
    archetype(701,
              RhythmFamily::MachineSyncopation,
              kKickBackbeatHatsPerc,
              kStaggeredMachineLanes, 5,
              kStaggeredMachineRelationships, 1,
              DensityContract{6, 10, 18, 5},
              straightTiming()),
    archetype(702,
              RhythmFamily::Funk16,
              kKickPerc,
              kCrossCycleLanes, 2,
              kCrossCycleRelationships, 1,
              DensityContract{6, 10, 13, 3},
              swingCompatible(
                  stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                  rhythmRoleBit(RhythmRole::Kick) |
                      rhythmRoleBit(RhythmRole::Percussion))),
    archetype(703,
              RhythmFamily::Breakbeat,
              kKickBackbeatHats,
              kBreakHalfstepLanes, 4,
              kBreakHalfstepRelationships, 1,
              DensityContract{5, 8, 11, 3},
              straightTiming()),
    archetype(704,
              RhythmFamily::HipHopBackbeat,
              kKickBackbeatHats,
              kRockPushLanes, 4,
              kRockPushRelationships, 1,
              DensityContract{8, 12, 15, 3},
              straightTiming()),
    archetype(705,
              RhythmFamily::SparsePulse,
              kKickBackbeatHatsPerc,
              kHalfbackControlLanes, 5,
              kHalfbackControlRelationships, 1,
              DensityContract{3, 5, 12, 4},
              straightTiming()),
};

constexpr Definition kDefinitions[] = {
    {Candidate::StaggeredMachine, 701, "staggered_machine", "HARD_02",
     EvidenceClass::MultiProvenanceReview, 118},
    {Candidate::CrossCycle, 702, "cross_cycle", "HARD_05",
     EvidenceClass::MultiProvenanceReview, 105},
    {Candidate::BreakHalfstep, 703, "break_halfstep", "HARD_04",
     EvidenceClass::SingleRootChallenger, 150},
    {Candidate::RockPush, 704, "rock_push", "HARD_09",
     EvidenceClass::SingleRootChallenger, 124},
    {Candidate::HalfbackControl, 705, "halfback_control", "HARD_03",
     EvidenceClass::SingleRootControl, 112},
};

constexpr RhythmCatalogView kCatalog = {
    kArchetypes,
    static_cast<uint16_t>(sizeof(kArchetypes) / sizeof(kArchetypes[0])),
    &kStatementTrajectory,
    1};

static_assert(sizeof(kDefinitions) / sizeof(kDefinitions[0]) ==
                  static_cast<uint8_t>(Candidate::Count),
              "Stage 7A candidate table incomplete");

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

const char* evidenceName(EvidenceClass evidence) {
  switch (evidence) {
    case EvidenceClass::MultiProvenanceReview:
      return "EVID";
    case EvidenceClass::SingleRootChallenger:
      return "CHAL";
    case EvidenceClass::SingleRootControl:
      return "CTRL";
    default:
      return "?";
  }
}

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
