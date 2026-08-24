#ifndef GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_REALIZER_H
#define GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_REALIZER_H

#include <cstdint>
#include <type_traits>

#include "../generation_context.h"
#include "relationship_resolver.h"
#include "rhythm_catalog.h"
#include "rhythm_types.h"

namespace GroovePuterRhythm {

struct RoleRhythmPlan {
  StepMask structural = 0;
  StepMask secondary = 0;
  StepMask ghosts = 0;

  // GateClass::Normal is implicit for onsets not present in these masks.
  StepMask shortGate = 0;
  StepMask heldGate = 0;
  StepMask tieGate = 0;

  StepMask accents = 0;
};

struct RhythmBarPlan {
  // Stage 2 establishes the independently realized bar. The same realizer
  // owner also applies any later caller-planned BarFunction mutation through
  // applyRhythmBarFunctionMutation().
  BarFunction function = BarFunction::Statement;
  RoleRhythmPlan roles[kRhythmRoleCount]{};
};

struct RhythmPhrasePlan {
  uint8_t barCount = 0;
  // Stage 2 does not select/pin trajectories or TransformationIntent. Keep the
  // fields explicit so Phrase/BarEvolution planning has a stable plan surface.
  TrajectoryId trajectoryId = kNoTrajectoryId;
  RealizationLevel level = RealizationLevel::P1Canonical;
  TransformationIntent intent = TransformationIntent::Auto;
  RhythmBarPlan bars[kMaxPhraseBars]{};
};

struct RhythmRealizationRequest {
  const RhythmCatalogView* catalog = nullptr;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  uint8_t phraseBars = 0;
  RealizationLevel level = RealizationLevel::P1Canonical;
  GenerationContext generation{};

  // Reuse this identity for VARIATE semantics. nullptr establishes a new
  // deterministic identity. The realizer never mutates the supplied identity.
  const PhraseRhythmIdentity* reuseIdentity = nullptr;
};

struct RhythmRealizationResult {
  RealizationStatus status = RealizationStatus::InvalidConstraintSet;
  PhraseRhythmIdentity identity{};
  RhythmPhrasePlan plan{};
};

// E2c canonical, bar-local mutation vocabulary. This is a value contract only:
// it does not allocate mutation lifecycle state and it does not execute a
// mutation. The single RhythmRole field is both source and target lane
// identity, so DISPLACE cannot express a cross-lane move.
enum class RhythmMutationOp : uint8_t {
  KEEP = 0,
  ADD = 1,
  DROP = 2,
  DISPLACE = 3,
  ACCENT = 4,
  GHOST = 5,
  Count = 6,
};

constexpr uint8_t kNoMutationStep = 0xFFu;

// E2c intentionally freezes a bar-local, non-wrapping displacement radius.
// Logical step 15 -> 0 is therefore not a radius-1 displacement.
constexpr uint8_t kDisplaceRadius = 2u;

// A lane/step may contribute one onset/topology classification plus one
// independent ACCENT change. These are absolute contract ceilings, not a
// producer quota and not a replacement for MutationBudget.
constexpr uint16_t kMaxRhythmMutationDeltasPerBar =
    static_cast<uint16_t>(kRhythmRoleCount) *
    static_cast<uint16_t>(kStepsPerBar) * 2u;
constexpr uint16_t kMaxRhythmMutationDeltasPerPhrase =
    static_cast<uint16_t>(kMaxRhythmMutationDeltasPerBar) *
    static_cast<uint16_t>(kMaxPhraseBars);

struct RhythmMutationDelta {
  RhythmMutationOp operation = RhythmMutationOp::KEEP;
  RhythmRole role = RhythmRole::Kick;
  uint8_t sourceStep = kNoMutationStep;
  uint8_t targetStep = kNoMutationStep;
};

// E2a producer surface. The producer enumerates one-hop, bar-local candidate
// deltas under rhythm_realizer ownership. It does not decide whether evolution
// should run, rank/select a candidate, or account canonical-relative mutation
// cost. E2b owns canonical diff/budget legality.
enum class RhythmMutationProducerStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  Count,
};

struct RhythmMutationProducerRequest {
  const RhythmArchetype* archetype = nullptr;
  const RhythmPhrasePlan* canonical = nullptr;
  const RhythmPhrasePlan* current = nullptr;
  uint8_t bar = 0;
  RhythmRoleMask roles = kAllRhythmRoles;
  BarFunction function = BarFunction::Statement;
  TransformationIntent intent = TransformationIntent::Auto;
  RealizationLevel level = RealizationLevel::P1Canonical;
  GenerationContext generation{};
};

struct RhythmMutationProducerResult {
  RhythmMutationProducerStatus status =
      RhythmMutationProducerStatus::InvalidRequest;
  uint16_t count = 0;
  bool truncated = false;
};

// E3a exact one-delta materialization result. Only DROP and DISPLACE are
// executable in this checkpoint. Selection, cadence and canonical-relative
// budget legality remain outside this surface.
enum class RhythmMutationApplyStatus : uint8_t {
  Success = 0,
  InvalidRequest,
  InvalidDelta,
  UnsupportedOperation,
  InvalidSource,
  UnsupportedSourceKind,
  OccupiedTarget,
  GrammarRejected,
  InvalidResult,
  Count,
};

constexpr bool rhythmMutationStepValid(uint8_t step) {
  return step < kStepsPerBar;
}

constexpr bool rhythmMutationRoleValid(RhythmRole role) {
  return static_cast<uint8_t>(role) < kRhythmRoleCount;
}

constexpr uint8_t rhythmMutationDisplacementDistance(
    uint8_t sourceStep,
    uint8_t targetStep) {
  return sourceStep >= targetStep
             ? static_cast<uint8_t>(sourceStep - targetStep)
             : static_cast<uint8_t>(targetStep - sourceStep);
}

// Frozen source/target shape:
// KEEP     source == target: an onset remains at the same logical coordinate.
// ADD      no source -> target: a non-ghost onset is added.
// DROP     source -> no target: an onset is removed; ghost removal is DROP.
// DISPLACE source -> target: same role/lane, distinct bar-local coordinates.
// ACCENT   source == target: accent state changes, onset topology does not.
// GHOST    no source -> target: a new ghost onset is added on an empty site.
//
// E2c does not encode the direction of ACCENT state in this topology record;
// producer/diff code derives it from the source/result plan. Current production
// addVariation() already treats ghosts as separate added onsets, not as an
// attribute conversion of an existing onset.
constexpr bool rhythmMutationDeltaShapeValid(const RhythmMutationDelta& delta) {
  if (!rhythmMutationRoleValid(delta.role)) return false;

  const bool hasSource = rhythmMutationStepValid(delta.sourceStep);
  const bool hasTarget = rhythmMutationStepValid(delta.targetStep);
  switch (delta.operation) {
    case RhythmMutationOp::KEEP:
      return hasSource && hasTarget &&
             delta.sourceStep == delta.targetStep;
    case RhythmMutationOp::ADD:
      return delta.sourceStep == kNoMutationStep && hasTarget;
    case RhythmMutationOp::DROP:
      return hasSource && delta.targetStep == kNoMutationStep;
    case RhythmMutationOp::DISPLACE:
      return hasSource && hasTarget &&
             delta.sourceStep != delta.targetStep &&
             rhythmMutationDisplacementDistance(
                 delta.sourceStep, delta.targetStep) <= kDisplaceRadius;
    case RhythmMutationOp::ACCENT:
      return hasSource && hasTarget &&
             delta.sourceStep == delta.targetStep;
    case RhythmMutationOp::GHOST:
      return delta.sourceStep == kNoMutationStep && hasTarget;
    case RhythmMutationOp::Count:
      return false;
  }
  return false;
}

// Canonical deterministic ordering is lane, logical coordinate, operation,
// source, target. For source-less ADD/GHOST the target is the coordinate.
// Logical step order is 0..15 and intentionally does not follow mask bit order.
constexpr uint8_t rhythmMutationCanonicalStep(
    const RhythmMutationDelta& delta) {
  return rhythmMutationStepValid(delta.sourceStep)
             ? delta.sourceStep
             : delta.targetStep;
}

constexpr bool rhythmMutationDeltaLess(const RhythmMutationDelta& lhs,
                                       const RhythmMutationDelta& rhs) {
  const uint8_t lhsRole = static_cast<uint8_t>(lhs.role);
  const uint8_t rhsRole = static_cast<uint8_t>(rhs.role);
  if (lhsRole != rhsRole) return lhsRole < rhsRole;

  const uint8_t lhsStep = rhythmMutationCanonicalStep(lhs);
  const uint8_t rhsStep = rhythmMutationCanonicalStep(rhs);
  if (lhsStep != rhsStep) return lhsStep < rhsStep;

  const uint8_t lhsOp = static_cast<uint8_t>(lhs.operation);
  const uint8_t rhsOp = static_cast<uint8_t>(rhs.operation);
  if (lhsOp != rhsOp) return lhsOp < rhsOp;

  if (lhs.sourceStep != rhs.sourceStep) {
    return lhs.sourceStep < rhs.sourceStep;
  }
  return lhs.targetStep < rhs.targetStep;
}

// Mutation budgeting remains external to the delta value. E2c deliberately
// adds no cost field, budget enum, or permission flags: E2a/E2b must consume
// the existing MutationBudget/MutationFlags and source/result plan context.

// Grammar-level DISPLACE legality. Budget/flag eligibility remains separate:
// this function answers only whether the lane grammar permits the source and
// target coordinates for the supplied planning context.
//
// Immutable anchors are never movable. Canonical anchors are movable only when
// a matching existing AnchorTransformRule explicitly names the source in
// displaceableCanonical. Non-anchor source and every target must live in the
// lane's preferred/optional onset space. Protected/forbidden coordinates and
// anchor targets are never legal displacement destinations.
inline bool rhythmMutationDisplacementGrammarLegal(
    const RhythmArchetype& archetype,
    const RhythmMutationDelta& delta,
    BarFunction function,
    TransformationIntent intent) {
  if (delta.operation != RhythmMutationOp::DISPLACE ||
      !rhythmMutationDeltaShapeValid(delta) ||
      archetype.laneCount > kMaxLanes ||
      archetype.protectedSpaceCount > kMaxProtectedSpaces ||
      archetype.anchorTransformRuleCount > kMaxAnchorTransformRules ||
      (archetype.laneCount != 0 && !archetype.lanes) ||
      (archetype.protectedSpaceCount != 0 && !archetype.protectedSpaces) ||
      (archetype.anchorTransformRuleCount != 0 &&
       !archetype.anchorTransformRules)) {
    return false;
  }

  const LaneGrammar* lane = nullptr;
  for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
    if (archetype.lanes[laneIndex].role == delta.role) {
      lane = &archetype.lanes[laneIndex];
      break;
    }
  }
  if (!lane) return false;

  const StepMask sourceBit = stepBit(delta.sourceStep);
  const StepMask targetBit = stepBit(delta.targetStep);
  StepMask protectedSteps = 0;
  const RhythmRoleMask roleMask = rhythmRoleBit(delta.role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    if (archetype.protectedSpaces[index].affectedRoles & roleMask) {
      protectedSteps = static_cast<StepMask>(
          protectedSteps | archetype.protectedSpaces[index].steps);
    }
  }

  if (((sourceBit | targetBit) & protectedSteps) ||
      ((sourceBit | targetBit) & lane->forbidden) ||
      (sourceBit & lane->immutableAnchors) ||
      (targetBit & (lane->immutableAnchors | lane->canonicalAnchors)) ||
      !(targetBit & (lane->preferred | lane->optional))) {
    return false;
  }

  if (sourceBit & lane->canonicalAnchors) {
    for (uint8_t index = 0;
         index < archetype.anchorTransformRuleCount;
         ++index) {
      const AnchorTransformRule& rule =
          archetype.anchorTransformRules[index];
      if (rule.role == delta.role &&
          rule.barFunction == function &&
          rule.intent == intent &&
          (rule.displaceableCanonical & sourceBit)) {
        return true;
      }
    }
    return false;
  }

  return (sourceBit & (lane->preferred | lane->optional)) != 0;
}

// Bounded, allocation-free E2a candidate producer. Output is a canonical-order
// prefix capped by both caller capacity and kMaxRhythmMutationDeltasPerBar.
// `truncated` reports that additional structurally possible proposals existed.
// GenerationContext is part of the stable input contract; E2a performs no
// stochastic pruning/ranking, so it does not use generation coordinates to
// reorder or select candidates.
RhythmMutationProducerResult produceRhythmMutationCandidates(
    const RhythmMutationProducerRequest& request,
    RhythmMutationDelta* output,
    uint16_t capacity);

RhythmRealizationResult realizeRhythmPhrase(
    const RhythmRealizationRequest& request);

PhraseOccupancy structuralOccupancy(const RhythmPhrasePlan& plan);

bool planRespectsProtectedSpace(const RhythmArchetype& archetype,
                                const RhythmPhrasePlan& plan);

bool planRespectsLaneBounds(const RhythmArchetype& archetype,
                            const RhythmPhrasePlan& plan);

// Exact E3a one-delta executor. It materializes only already-approved DROP or
// DISPLACE deltas. It owns no selection, cadence, lifecycle or canonical
// budget accounting. Any non-Success result leaves `plan` unchanged.
RhythmMutationApplyStatus applyRhythmMutationDelta(
    const RhythmArchetype& archetype,
    RhythmPhrasePlan& plan,
    uint8_t bar,
    BarFunction function,
    TransformationIntent intent,
    const RhythmMutationDelta& delta);

// Canonical rhythm mutation owner for a caller-planned BarFunction. This is
// the only production executor for trajectory/reduction/build/break mutation.
// It preserves the existing deterministic BarEvolution semantics while keeping
// planning (trajectory and BarFunction choice) outside the mutation owner.
bool applyRhythmBarFunctionMutation(const RhythmArchetype& archetype,
                                    RhythmPhrasePlan& plan,
                                    uint8_t bar,
                                    BarFunction function,
                                    uint32_t seed);

// Validation surface for plans after canonical BarFunction mutation. The
// compatibility BarEvolution API delegates its historical evolvedPlanValid()
// entry point to this realizer-owned implementation.
bool rhythmMutationPlanValid(const RhythmArchetype& archetype,
                             const RhythmPhrasePlan& plan);

static_assert(static_cast<uint8_t>(RhythmMutationOp::KEEP) == 0,
              "RhythmMutationOp KEEP ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationOp::ADD) == 1,
              "RhythmMutationOp ADD ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationOp::DROP) == 2,
              "RhythmMutationOp DROP ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationOp::DISPLACE) == 3,
              "RhythmMutationOp DISPLACE ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationOp::ACCENT) == 4,
              "RhythmMutationOp ACCENT ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationOp::GHOST) == 5,
              "RhythmMutationOp GHOST ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationProducerStatus::Ok) == 0,
              "RhythmMutationProducerStatus Ok ordinal changed");
static_assert(static_cast<uint8_t>(RhythmMutationApplyStatus::Success) == 0,
              "RhythmMutationApplyStatus Success ordinal changed");
static_assert(static_cast<uint8_t>(GenerationDomain::BarEvolution) == 12,
              "GenerationDomain ABI changed: BarEvolution must remain 12");
static_assert(kDisplaceRadius == 2,
              "E2c canonical displacement radius changed");
static_assert(kMaxRhythmMutationDeltasPerBar == 256,
              "E2c per-bar mutation-delta bound changed");
static_assert(kMaxRhythmMutationDeltasPerPhrase == 1024,
              "E2c per-phrase mutation-delta bound changed");
static_assert(std::is_trivially_copyable<RhythmMutationDelta>::value,
              "RhythmMutationDelta must remain fixed-capacity");
static_assert(sizeof(RhythmMutationDelta) == 4,
              "RhythmMutationDelta representation changed");
static_assert(std::is_trivially_copyable<RhythmMutationProducerRequest>::value,
              "RhythmMutationProducerRequest must remain allocation-free");
static_assert(std::is_trivially_copyable<RhythmMutationProducerResult>::value,
              "RhythmMutationProducerResult must remain allocation-free");
static_assert(std::is_trivially_copyable<RoleRhythmPlan>::value,
              "RoleRhythmPlan must remain fixed-capacity");
static_assert(std::is_trivially_copyable<RhythmPhrasePlan>::value,
              "RhythmPhrasePlan must remain fixed-capacity");
static_assert(sizeof(RhythmPhrasePlan) <= 640,
              "RhythmPhrasePlan exceeded the Stage 2 bounded RAM target");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_REALIZER_H
