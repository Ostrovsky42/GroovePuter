#ifndef GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_TYPES_H
#define GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_TYPES_H

#include <cstdint>
#include <type_traits>

namespace GroovePuterRhythm {

using StepMask = uint16_t;
using RhythmRoleMask = uint16_t;
using RhythmArchetypeId = uint16_t;
using TrajectoryId = uint8_t;
using PhraseBarsMask = uint8_t;
using RealizationLevelMask = uint8_t;
using TransformationIntentMask = uint8_t;

constexpr uint8_t kStepsPerBar = 16;
constexpr uint8_t kMaxPhraseBars = 4;
constexpr uint8_t kRhythmRoleCount = 8;
constexpr uint8_t kMaxLanes = kRhythmRoleCount;
constexpr uint8_t kMaxProtectedSpaces = 8;
constexpr uint8_t kMaxRelationships = 16;
constexpr uint8_t kMaxAnchorTransformRules = 8;
constexpr uint8_t kMaxTrajectoryRefs = 8;
constexpr StepMask kAllSteps = 0xFFFFu;

// Keep the repository's established 16-step mask convention:
// logical step 0 is bit 15, logical step 15 is bit 0.
constexpr StepMask stepBit(uint8_t step) {
  return step < kStepsPerBar
             ? static_cast<StepMask>(1u << (kStepsPerBar - 1u - step))
             : 0u;
}

constexpr uint16_t kNoArchetypeIndex = 0xFFFFu;
constexpr uint8_t kNoItemIndex = 0xFFu;
constexpr RhythmArchetypeId kNoArchetypeId = 0u;
constexpr TrajectoryId kNoTrajectoryId = 0u;

enum class RhythmFamily : uint8_t {
  FourFloor = 0,
  MachineSyncopation,
  Breakbeat,
  UkTwoStep,
  HipHopBackbeat,
  DubPulse,
  Funk16,
  SparsePulse,
  Count,
};

enum class RhythmRole : uint8_t {
  Kick = 0,
  Backbeat,
  ClosedHat,
  OpenHat,
  Percussion,
  BassRhythm,
  ChordRhythm,
  MelodicRhythm,
  Count,
};

enum class GateClass : uint8_t {
  Short = 0,
  Normal,
  Held,
  Tie,
  Count,
};

enum class EventImportance : uint8_t {
  Structural = 0,
  Secondary,
  Ghost,
  Count,
};

enum class RelationshipOp : uint8_t {
  Exclude = 0,
  Coincide,
  Offset,
  Respond,
  FillGaps,
  Count,
};

enum class ConstraintStrength : uint8_t {
  Soft = 0,
  Hard,
  Count,
};

enum class RelationshipScope : uint8_t {
  BarLocal = 0,
  Phrase,
  Count,
};

enum class RealizationLevel : uint8_t {
  P1Canonical = 0,
  P2Variation,
  P3Transformation,
  Count,
};

enum class RealizationStatus : uint8_t {
  Ok = 0,
  ValidButSparse,
  InvalidConstraintSet,
  Count,
};

enum class TransformationIntent : uint8_t {
  Auto = 0,
  Fill,
  Reduce,
  Break,
  Build,
  Turnaround,
  Response,
  Count,
};

enum class TimingCompatibility : uint8_t {
  StraightOnly = 0,
  SwingCompatible,
  ShufflePreferred,
  Count,
};

enum class BarFunction : uint8_t {
  Statement = 0,
  Repeat,
  RepeatWithGhosts,
  Response,
  Reduction,
  Build,
  Turnaround,
  Break,
  Return,
  Count,
};

enum class GenerationDomain : uint8_t {
  ArchetypeSelection = 0,
  RhythmIdentity,
  P1Variation,
  P2Variation,
  P3Transformation,
  TransformationIntent,
  DrumsOrnament,
  BassPitch,
  ChordPitch,
  LeadPitch,
  FeelExpression,
  PhysicalBinding,
  BarEvolution,
  BassRhythmSelection,
  ChordRhythmSelection,
  MelodicRhythmSelection,
  MotifSelection,
  FeelProfileSelection,
  PhraseLawSelection,
  Count,
};

constexpr RhythmRoleMask rhythmRoleBit(RhythmRole role) {
  const uint8_t index = static_cast<uint8_t>(role);
  return index < kRhythmRoleCount
             ? static_cast<RhythmRoleMask>(1u << index)
             : 0u;
}

constexpr RhythmRoleMask kAllRhythmRoles =
    static_cast<RhythmRoleMask>((1u << kRhythmRoleCount) - 1u);

constexpr PhraseBarsMask phraseBarsBit(uint8_t bars) {
  return bars >= 1 && bars <= kMaxPhraseBars
             ? static_cast<PhraseBarsMask>(1u << (bars - 1u))
             : 0u;
}

constexpr PhraseBarsMask kAllPhraseBars =
    static_cast<PhraseBarsMask>((1u << kMaxPhraseBars) - 1u);

constexpr RealizationLevelMask realizationLevelBit(RealizationLevel level) {
  const uint8_t index = static_cast<uint8_t>(level);
  return index < static_cast<uint8_t>(RealizationLevel::Count)
             ? static_cast<RealizationLevelMask>(1u << index)
             : 0u;
}

constexpr RealizationLevelMask kAllRealizationLevels =
    static_cast<RealizationLevelMask>(
        (1u << static_cast<uint8_t>(RealizationLevel::Count)) - 1u);

constexpr TransformationIntentMask transformationIntentBit(
    TransformationIntent intent) {
  const uint8_t index = static_cast<uint8_t>(intent);
  return index < static_cast<uint8_t>(TransformationIntent::Count)
             ? static_cast<TransformationIntentMask>(1u << index)
             : 0u;
}

constexpr TransformationIntentMask kConcreteTransformationIntents =
    static_cast<TransformationIntentMask>(
        transformationIntentBit(TransformationIntent::Fill) |
        transformationIntentBit(TransformationIntent::Reduce) |
        transformationIntentBit(TransformationIntent::Break) |
        transformationIntentBit(TransformationIntent::Build) |
        transformationIntentBit(TransformationIntent::Turnaround) |
        transformationIntentBit(TransformationIntent::Response));

constexpr TransformationIntentMask kP2TransformationIntents =
    static_cast<TransformationIntentMask>(
        transformationIntentBit(TransformationIntent::Reduce) |
        transformationIntentBit(TransformationIntent::Response));

struct LaneGrammar {
  RhythmRole role = RhythmRole::Kick;

  // These five spaces are mutually exclusive in Core v1.
  StepMask immutableAnchors = 0;
  StepMask canonicalAnchors = 0;
  StepMask preferred = 0;
  StepMask optional = 0;
  StepMask forbidden = 0;

  // Gate overlays are coordinate policies over declared legal onset
  // space. Normal is implicit when an onset is not present in any
  // explicit gate mask. The three masks must be mutually exclusive.
  StepMask shortGate = 0;
  StepMask heldGate = 0;
  StepMask tieGate = 0;

  uint8_t structuralMin = 0;
  uint8_t structuralMax = 0;
  uint8_t ornamentMax = 0;

  uint8_t accentProfileId = 0;
  uint8_t flags = 0;
};

struct ProtectedSpace {
  StepMask steps = 0;
  RhythmRoleMask affectedRoles = 0;
};

struct RhythmEventIntent {
  uint8_t step = 0;
  GateClass gate = GateClass::Normal;
  EventImportance importance = EventImportance::Structural;
  uint8_t accentClass = 0;
};

struct LaneRelationship {
  RhythmRole source = RhythmRole::Kick;
  RhythmRole target = RhythmRole::Backbeat;
  RelationshipOp op = RelationshipOp::Exclude;
  ConstraintStrength strength = ConstraintStrength::Hard;
  RelationshipScope scope = RelationshipScope::BarLocal;

  StepMask zoneMask = kAllSteps;

  // Signed source->target window. Positive values mean target occurs after
  // source. Used only by Offset/Respond in Core v1.
  int8_t minOffset = 0;
  int8_t maxOffset = 0;

  // Coincide-specific phrase-level cardinality. maxMatches == 0 means no
  // explicit upper bound beyond lane/density budgets.
  uint8_t minMatches = 0;
  uint8_t maxMatches = 0;

  // Respond-specific cardinality per source response window.
  // maxResponsesPerWindow == 0 means no explicit upper bound.
  uint8_t minResponsesPerWindow = 0;
  uint8_t maxResponsesPerWindow = 0;

  // Soft relationships require a non-zero weight. Hard relationships use 0.
  uint8_t weight = 0;
};

struct TimingEligibility {
  TimingCompatibility compatibility = TimingCompatibility::StraightOnly;
  StepMask sensitiveSteps = 0;
  RhythmRoleMask affectedRoles = 0;
};

struct DensityContract {
  uint8_t structuralMin = 0;
  uint8_t structuralPreferred = 0;
  uint8_t structuralMax = 0;
  uint8_t ornamentMax = 0;
};

enum MutationFlags : uint16_t {
  AllowOptionalAdds = 1u << 0,
  AllowPreferredDrops = 1u << 1,
  AllowGhostConversion = 1u << 2,
  AllowOptionalDisplace = 1u << 3,
  AllowAccentVariation = 1u << 4,
  AllowReduction = 1u << 5,
  AllowTurnaround = 1u << 6,
  AllowBreak = 1u << 7,
};

constexpr uint16_t kAllMutationFlags =
    AllowOptionalAdds | AllowPreferredDrops | AllowGhostConversion |
    AllowOptionalDisplace | AllowAccentVariation | AllowReduction |
    AllowTurnaround | AllowBreak;

struct MutationBudget {
  uint8_t maxAdds = 0;
  uint8_t maxDrops = 0;
  uint8_t maxDisplacements = 0;
  uint8_t maxAccentChanges = 0;
  uint16_t flags = 0;
  TransformationIntentMask allowedIntents = 0;
};

struct MutationPolicy {
  MutationBudget level[static_cast<uint8_t>(RealizationLevel::Count)]{};
};

struct BarTrajectory {
  TrajectoryId id = kNoTrajectoryId;
  uint8_t barCount = 0;
  BarFunction bars[kMaxPhraseBars]{};
};

struct TrajectoryRef {
  TrajectoryId id = kNoTrajectoryId;
  uint8_t weight = 0;
  RealizationLevelMask allowedLevels = 0;
};

struct AnchorTransformRule {
  RhythmRole role = RhythmRole::Kick;
  BarFunction barFunction = BarFunction::Break;
  TransformationIntent intent = TransformationIntent::Break;
  StepMask suppressibleCanonical = 0;
  StepMask displaceableCanonical = 0;
};

struct RhythmArchetype {
  RhythmArchetypeId id = kNoArchetypeId;
  RhythmFamily family = RhythmFamily::FourFloor;
  PhraseBarsMask allowedPhraseBars = 0;
  RhythmRoleMask activeRoles = 0;

  const LaneGrammar* lanes = nullptr;
  uint8_t laneCount = 0;

  const ProtectedSpace* protectedSpaces = nullptr;
  uint8_t protectedSpaceCount = 0;

  const LaneRelationship* relationships = nullptr;
  uint8_t relationshipCount = 0;

  const AnchorTransformRule* anchorTransformRules = nullptr;
  uint8_t anchorTransformRuleCount = 0;

  const TrajectoryRef* trajectories = nullptr;
  uint8_t trajectoryCount = 0;

  TimingEligibility timing{};
  DensityContract density{};
  MutationPolicy mutation{};
};

struct PhraseRhythmIdentity {
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  uint8_t phraseBars = 0;
  TrajectoryId trajectoryId = kNoTrajectoryId;

  // structuralCore contains the stable selected onsets for phrase identity.
  // canonicalCore is the subset that may be transformed only by explicit
  // BarFunction + TransformationIntent + AnchorTransformRule permission.
  StepMask structuralCore[kMaxPhraseBars][kRhythmRoleCount]{};
  StepMask canonicalCore[kMaxPhraseBars][kRhythmRoleCount]{};

  ProtectedSpace protectedSpaces[kMaxProtectedSpaces]{};
  uint8_t protectedSpaceCount = 0;
};

static_assert(kStepsPerBar == 16, "Groove Vocabulary Core v1 grid changed");
static_assert(kMaxPhraseBars == 4, "Groove Vocabulary Core v1 phrase bound changed");
static_assert(kRhythmRoleCount <= 16, "RhythmRoleMask no longer fits uint16_t");
static_assert(sizeof(PhraseRhythmIdentity) <= 256,
              "PhraseRhythmIdentity must remain compact and bounded");
static_assert(std::is_trivially_copyable<LaneGrammar>::value,
              "LaneGrammar must remain a fixed-capacity value");
static_assert(std::is_trivially_copyable<PhraseRhythmIdentity>::value,
              "PhraseRhythmIdentity must remain a fixed-capacity value");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_TYPES_H
