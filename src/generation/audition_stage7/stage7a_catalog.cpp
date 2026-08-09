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

// Batch 2 is deliberately more adversarial than Batch 1. Pass #2 classifies
// HARD_01/06/07/08 as HOLD_SINGLE_ROOT, so these are listening hypotheses,
// not reconstructions of source topologies and not production candidates.
// Each grammar keeps a small generic identity core and gives the realizer a
// wider legal corridor. If listening says "same as existing", that is a valid
// falsification result.

// HARD_01: Basic/House boundary. Quarter-stack behavior is allowed but not
// forced as a complete four-on-floor source mask; backbeat may coincide with
// kick because the aggregate directions disagree on that relationship.
constexpr LaneGrammar kStackedQuartersLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(8),
         stepBit(4) | stepBit(12),
         stepBit(6) | stepBit(14),
         2, 5),
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
         0, 0,
         stepBit(6) | stepBit(14),
         0, 2),
    lane(RhythmRole::Percussion,
         0, 0,
         stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
         0, 2, 1),
};

// HARD_06: Electro/Hip-Hop/Miami Bass single-root region. The test hypothesis
// is a staggered low-drum line around a fixed backbeat with broad hat support.
constexpr LaneGrammar kElectroBackskipLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(10),
         stepBit(3) | stepBit(6) | stepBit(13),
         stepBit(8) | stepBit(15),
         2, 5),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
         2, 6, 2),
    lane(RhythmRole::OpenHat,
         0, 0,
         stepBit(6) | stepBit(14),
         0, 2),
    lane(RhythmRole::Percussion,
         0,
         stepBit(1) | stepBit(9),
         stepBit(5) | stepBit(13),
         0, 3, 2),
};
constexpr LaneRelationship kElectroBackskipRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

// HARD_07: Funk/Soul + House boundary. The identity lives in a stable
// backbeat plus syncopated kick/percussion choices; no literal funk source
// skeleton is embedded. Kick/backbeat is deliberately not a hard Exclude:
// Funk/Soul aggregate evidence favors gaps while House favors coincidence.
constexpr LaneGrammar kFunkHouseBridgeLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(8),
         stepBit(3) | stepBit(10) | stepBit(14),
         stepBit(2) | stepBit(6) | stepBit(15),
         2, 5),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         stepBit(1) | stepBit(5) | stepBit(9) | stepBit(13),
         2, 6, 2),
    lane(RhythmRole::OpenHat,
         0, 0,
         stepBit(6) | stepBit(14),
         0, 2),
    lane(RhythmRole::Percussion,
         0,
         stepBit(7) | stepBit(15),
         stepBit(3) | stepBit(11),
         1, 3, 2),
};

// HARD_08: second Electro/Hip-Hop/Miami Bass single-root region. It is kept
// intentionally sparser than HARD_06 so listening can determine whether Pass
// #2 separated two real musical ideas or merely split one broad family.
constexpr LaneGrammar kElectroGapPushLanes[] = {
    lane(RhythmRole::Kick,
         stepBit(0) | stepBit(6),
         stepBit(9) | stepBit(14),
         stepBit(3) | stepBit(11) | stepBit(15),
         2, 5),
    lane(RhythmRole::Backbeat,
         stepBit(4) | stepBit(12),
         0, 0,
         2, 2),
    lane(RhythmRole::ClosedHat,
         0,
         stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
         stepBit(0) | stepBit(8),
         2, 5, 2),
    lane(RhythmRole::OpenHat,
         0, 0,
         stepBit(2) | stepBit(10),
         0, 2),
    lane(RhythmRole::Percussion,
         0,
         stepBit(7) | stepBit(15),
         stepBit(1) | stepBit(9) | stepBit(13),
         0, 3, 2),
};
constexpr LaneRelationship kElectroGapPushRelationships[] = {
    hardExclude(RhythmRole::Backbeat, RhythmRole::Kick),
};

constexpr RhythmArchetype kArchetypes[] = {
    archetype(711,
              RhythmFamily::FourFloor,
              kKickBackbeatHatsPerc,
              kStackedQuartersLanes, 5,
              nullptr, 0,
              DensityContract{6, 9, 16, 4},
              straightTiming()),
    archetype(712,
              RhythmFamily::MachineSyncopation,
              kKickBackbeatHatsPerc,
              kElectroBackskipLanes, 5,
              kElectroBackskipRelationships, 1,
              DensityContract{7, 10, 18, 5},
              straightTiming()),
    archetype(713,
              RhythmFamily::Funk16,
              kKickBackbeatHatsPerc,
              kFunkHouseBridgeLanes, 5,
              nullptr, 0,
              DensityContract{7, 11, 18, 5},
              swingCompatible(
                  stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
                  rhythmRoleBit(RhythmRole::Kick) |
                      rhythmRoleBit(RhythmRole::ClosedHat) |
                      rhythmRoleBit(RhythmRole::Percussion))),
    archetype(714,
              RhythmFamily::HipHopBackbeat,
              kKickBackbeatHatsPerc,
              kElectroGapPushLanes, 5,
              kElectroGapPushRelationships, 1,
              DensityContract{6, 9, 17, 5},
              straightTiming()),
};

constexpr Definition kDefinitions[] = {
    {Candidate::StackedQuarters, 711, "stacked_quarters", "HARD_01",
     EvidenceClass::SingleRootChallenger, 122},
    {Candidate::ElectroBackskip, 712, "electro_backskip", "HARD_06",
     EvidenceClass::SingleRootChallenger, 116},
    {Candidate::FunkHouseBridge, 713, "funk_house_bridge", "HARD_07",
     EvidenceClass::SingleRootChallenger, 112},
    {Candidate::ElectroGapPush, 714, "electro_gap_push", "HARD_08",
     EvidenceClass::SingleRootChallenger, 114},
};

constexpr RhythmCatalogView kCatalog = {
    kArchetypes,
    static_cast<uint16_t>(sizeof(kArchetypes) / sizeof(kArchetypes[0])),
    &kStatementTrajectory,
    1};

static_assert(sizeof(kDefinitions) / sizeof(kDefinitions[0]) ==
                  static_cast<uint8_t>(Candidate::Count),
              "Stage 7B candidate table incomplete");

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
