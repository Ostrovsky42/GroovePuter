#include "rhythm_catalog.h"

#include <cstddef>
#include <cstdint>

namespace GroovePuterRhythm {
namespace {

constexpr int kMaxPhraseOffset = kStepsPerBar * kMaxPhraseBars - 1;
constexpr uint16_t kMaxEventsPerBar = kStepsPerBar * kMaxLanes;

template <typename T>
bool validEnum(T value, T count) {
  return static_cast<uint8_t>(value) < static_cast<uint8_t>(count);
}

CatalogValidationResult fail(CatalogValidationError error,
                             uint16_t archetype = kNoArchetypeIndex,
                             uint8_t item = kNoItemIndex) {
  return {error, archetype, item};
}

uint8_t bitCount16(StepMask mask) {
  uint8_t count = 0;
  while (mask) {
    mask = static_cast<StepMask>(mask & (mask - 1u));
    ++count;
  }
  return count;
}

uint8_t bitCount64(uint64_t mask) {
  uint8_t count = 0;
  while (mask) {
    mask &= mask - 1u;
    ++count;
  }
  return count;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    if (archetype.lanes[i].role == role) return &archetype.lanes[i];
  }
  return nullptr;
}

StepMask anchors(const LaneGrammar& lane) {
  return static_cast<StepMask>(
      lane.immutableAnchors | lane.canonicalAnchors);
}

StepMask protectedFor(const RhythmArchetype& archetype, RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask roleMask = rhythmRoleBit(role);
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    if (archetype.protectedSpaces[i].affectedRoles & roleMask) {
      result = static_cast<StepMask>(
          result | archetype.protectedSpaces[i].steps);
    }
  }
  return result;
}

StepMask candidates(const RhythmArchetype& archetype,
                    const LaneGrammar& lane) {
  const StepMask positive = static_cast<StepMask>(
      anchors(lane) | lane.preferred | lane.optional);
  return static_cast<StepMask>(
      positive & ~lane.forbidden & ~protectedFor(archetype, lane.role));
}

uint64_t expand(StepMask mask, uint8_t bars) {
  uint64_t result = 0;
  for (uint8_t bar = 0; bar < bars; ++bar) {
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (mask & stepBit(step)) {
        result |= uint64_t{1} << (bar * kStepsPerBar + step);
      }
    }
  }
  return result;
}

uint64_t activePhraseMask(uint8_t bars) {
  if (bars == kMaxPhraseBars) return UINT64_MAX;
  return (uint64_t{1} << (bars * kStepsPerBar)) - 1u;
}

bool enoughPerBar(uint64_t mask, uint8_t bars, uint8_t minimum) {
  for (uint8_t bar = 0; bar < bars; ++bar) {
    const uint64_t barMask =
        uint64_t{0xFFFF} << (bar * kStepsPerBar);
    if (bitCount64(mask & barMask) < minimum) return false;
  }
  return true;
}

uint64_t reachableFrom(uint64_t sourceMask,
                       int8_t minOffset,
                       int8_t maxOffset,
                       RelationshipScope scope,
                       uint8_t bars) {
  uint64_t result = 0;
  const int totalSteps = bars * kStepsPerBar;
  for (int source = 0; source < totalSteps; ++source) {
    if (!(sourceMask & (uint64_t{1} << source))) continue;
    for (int delta = minOffset; delta <= maxOffset; ++delta) {
      const int target = source + delta;
      if (target < 0 || target >= totalSteps) continue;
      if (scope == RelationshipScope::BarLocal &&
          source / kStepsPerBar != target / kStepsPerBar) {
        continue;
      }
      result |= uint64_t{1} << target;
    }
  }
  return result;
}

uint64_t responseWindow(int source,
                        int8_t minOffset,
                        int8_t maxOffset,
                        RelationshipScope scope,
                        uint8_t bars) {
  uint64_t result = 0;
  const int totalSteps = bars * kStepsPerBar;
  for (int delta = minOffset; delta <= maxOffset; ++delta) {
    const int target = source + delta;
    if (target < 0 || target >= totalSteps) continue;
    if (scope == RelationshipScope::BarLocal &&
        source / kStepsPerBar != target / kStepsPerBar) {
      continue;
    }
    result |= uint64_t{1} << target;
  }
  return result;
}

int responseOwner(int target,
                  uint64_t sourceAnchors,
                  uint64_t zone,
                  const LaneRelationship& relation,
                  uint8_t bars) {
  int owner = -1;
  int bestDistance = kMaxPhraseOffset + 1;
  const int totalSteps = bars * kStepsPerBar;
  for (int source = 0; source < totalSteps; ++source) {
    const uint64_t sourceBit = uint64_t{1} << source;
    if (!(sourceAnchors & zone & sourceBit)) continue;
    if (!(responseWindow(source,
                         relation.minOffset,
                         relation.maxOffset,
                         relation.scope,
                         bars) &
          (uint64_t{1} << target))) {
      continue;
    }
    const int distance = target >= source ? target - source : source - target;
    if (distance < bestDistance ||
        (distance == bestDistance && (owner < 0 || source < owner))) {
      owner = source;
      bestDistance = distance;
    }
  }
  return owner;
}

bool mandatoryRespondFeasible(uint64_t sourceAnchors,
                              uint64_t targetCandidates,
                              uint64_t targetAnchors,
                              uint64_t zone,
                              const LaneRelationship& relation,
                              uint8_t bars) {
  uint8_t candidateOwned[kMaxPhraseBars * kStepsPerBar]{};
  uint8_t mandatoryOwned[kMaxPhraseBars * kStepsPerBar]{};
  const int totalSteps = bars * kStepsPerBar;

  for (int target = 0; target < totalSteps; ++target) {
    const uint64_t bit = uint64_t{1} << target;
    if (!(targetCandidates & bit)) continue;
    const int owner = responseOwner(target,
                                    sourceAnchors,
                                    zone,
                                    relation,
                                    bars);
    if (owner < 0) continue;
    ++candidateOwned[owner];
    if (targetAnchors & bit) ++mandatoryOwned[owner];
  }

  for (int source = 0; source < totalSteps; ++source) {
    const uint64_t bit = uint64_t{1} << source;
    if (!(sourceAnchors & zone & bit)) continue;
    if (candidateOwned[source] < relation.minResponsesPerWindow) return false;
    if (relation.maxResponsesPerWindow &&
        mandatoryOwned[source] > relation.maxResponsesPerWindow) {
      return false;
    }
  }
  return true;
}

bool excludeFeasible(const RhythmArchetype& archetype,
                     const LaneGrammar& source,
                     const LaneGrammar& target,
                     StepMask zone) {
  const StepMask sourceAnchors = anchors(source);
  const StepMask targetAnchors = anchors(target);
  if (sourceAnchors & targetAnchors & zone) return false;

  const StepMask sourceAvailable = static_cast<StepMask>(
      candidates(archetype, source) & ~(targetAnchors & zone));
  const StepMask targetAvailable = static_cast<StepMask>(
      candidates(archetype, target) & ~(sourceAnchors & zone));
  const uint8_t sourceNeed =
      source.structuralMin > bitCount16(sourceAnchors)
          ? source.structuralMin
          : bitCount16(sourceAnchors);
  const uint8_t targetNeed =
      target.structuralMin > bitCount16(targetAnchors)
          ? target.structuralMin
          : bitCount16(targetAnchors);

  if (bitCount16(sourceAvailable) < sourceNeed ||
      bitCount16(targetAvailable) < targetNeed) {
    return false;
  }

  const StepMask sharedConflict = static_cast<StepMask>(
      sourceAvailable & targetAvailable & zone);
  const uint8_t sourceNonConflict = bitCount16(
      static_cast<StepMask>(sourceAvailable & ~sharedConflict));
  const uint8_t targetNonConflict = bitCount16(
      static_cast<StepMask>(targetAvailable & ~sharedConflict));
  const uint8_t sourceSharedNeed =
      sourceNeed > sourceNonConflict ? sourceNeed - sourceNonConflict : 0;
  const uint8_t targetSharedNeed =
      targetNeed > targetNonConflict ? targetNeed - targetNonConflict : 0;

  return static_cast<uint16_t>(sourceSharedNeed) + targetSharedNeed <=
         bitCount16(sharedConflict);
}

bool coincideFeasible(const RhythmArchetype& archetype,
                      const LaneGrammar& source,
                      const LaneGrammar& target,
                      const LaneRelationship& relation,
                      uint8_t bars) {
  const StepMask sourceAnchors = anchors(source);
  const StepMask targetAnchors = anchors(target);
  const StepMask possible = static_cast<StepMask>(
      candidates(archetype, source) &
      candidates(archetype, target) &
      relation.zoneMask);
  const StepMask mandatory = static_cast<StepMask>(
      sourceAnchors & targetAnchors & relation.zoneMask);
  const StepMask sourceAnchoredOnly = static_cast<StepMask>(
      possible & sourceAnchors & ~targetAnchors);
  const StepMask targetAnchoredOnly = static_cast<StepMask>(
      possible & targetAnchors & ~sourceAnchors);
  const StepMask unanchoredShared = static_cast<StepMask>(
      possible & ~sourceAnchors & ~targetAnchors);

  const uint16_t sourceCapacity =
      static_cast<uint16_t>(source.structuralMax) * bars;
  const uint16_t targetCapacity =
      static_cast<uint16_t>(target.structuralMax) * bars;
  const uint16_t sourceAnchorCount =
      static_cast<uint16_t>(bitCount16(sourceAnchors)) * bars;
  const uint16_t targetAnchorCount =
      static_cast<uint16_t>(bitCount16(targetAnchors)) * bars;
  if (sourceAnchorCount > sourceCapacity ||
      targetAnchorCount > targetCapacity) {
    return false;
  }

  uint16_t sourceFree = sourceCapacity - sourceAnchorCount;
  uint16_t targetFree = targetCapacity - targetAnchorCount;
  const uint16_t mandatoryMatches =
      static_cast<uint16_t>(bitCount16(mandatory)) * bars;
  uint16_t maxMatches = mandatoryMatches;

  const uint16_t sourceAnchoredOnlyCount =
      static_cast<uint16_t>(bitCount16(sourceAnchoredOnly)) * bars;
  const uint16_t targetAnchoredOnlyCount =
      static_cast<uint16_t>(bitCount16(targetAnchoredOnly)) * bars;
  const uint16_t unanchoredSharedCount =
      static_cast<uint16_t>(bitCount16(unanchoredShared)) * bars;

  const uint16_t fromSourceAnchors =
      sourceAnchoredOnlyCount < targetFree
          ? sourceAnchoredOnlyCount
          : targetFree;
  maxMatches += fromSourceAnchors;
  targetFree -= fromSourceAnchors;

  const uint16_t fromTargetAnchors =
      targetAnchoredOnlyCount < sourceFree
          ? targetAnchoredOnlyCount
          : sourceFree;
  maxMatches += fromTargetAnchors;
  sourceFree -= fromTargetAnchors;

  uint16_t fromUnanchored = sourceFree < targetFree
                                 ? sourceFree
                                 : targetFree;
  if (fromUnanchored > unanchoredSharedCount) {
    fromUnanchored = unanchoredSharedCount;
  }
  maxMatches += fromUnanchored;

  if (maxMatches < relation.minMatches) return false;
  if (relation.maxMatches && mandatoryMatches > relation.maxMatches) {
    return false;
  }
  return true;
}

bool hardRelationshipFeasible(const RhythmArchetype& archetype,
                              const LaneRelationship& relation,
                              uint8_t bars) {
  const LaneGrammar* source = laneFor(archetype, relation.source);
  const LaneGrammar* target = laneFor(archetype, relation.target);
  if (!source || !target) return false;

  if (relation.op == RelationshipOp::Exclude) {
    return excludeFeasible(archetype, *source, *target, relation.zoneMask);
  }
  if (relation.op == RelationshipOp::Coincide) {
    return coincideFeasible(archetype, *source, *target, relation, bars);
  }

  uint64_t sourceCandidates = expand(candidates(archetype, *source), bars);
  uint64_t targetCandidates = expand(candidates(archetype, *target), bars);
  if (!source->structuralMax) sourceCandidates = 0;
  if (!target->structuralMax) targetCandidates = 0;
  const uint64_t sourceAnchors = expand(anchors(*source), bars);
  const uint64_t targetAnchors = expand(anchors(*target), bars);
  const uint64_t zone = expand(relation.zoneMask, bars);
  const uint64_t phraseMask = activePhraseMask(bars);

  if (relation.op == RelationshipOp::Offset) {
    const uint64_t reachable = reachableFrom(sourceCandidates,
                                             relation.minOffset,
                                             relation.maxOffset,
                                             relation.scope,
                                             bars);
    if (targetAnchors & zone & ~reachable) return false;
    return enoughPerBar(
        targetCandidates & ((~zone & phraseMask) | reachable),
        bars,
        target->structuralMin);
  }

  if (relation.op == RelationshipOp::Respond) {
    if (!mandatoryRespondFeasible(sourceAnchors,
                                  targetCandidates,
                                  targetAnchors,
                                  zone,
                                  relation,
                                  bars)) {
      return false;
    }
    if (!relation.minResponsesPerWindow) return true;

    uint64_t viableSources = 0;
    uint16_t mandatoryWindows = 0;
    const int totalSteps = bars * kStepsPerBar;
    for (int sourceStep = 0; sourceStep < totalSteps; ++sourceStep) {
      const uint64_t sourceBit = uint64_t{1} << sourceStep;
      if (!(sourceCandidates & sourceBit)) continue;
      const bool constrained = (zone & sourceBit) != 0;
      const uint8_t available = constrained
          ? bitCount64(targetCandidates & responseWindow(
                sourceStep,
                relation.minOffset,
                relation.maxOffset,
                relation.scope,
                bars))
          : relation.minResponsesPerWindow;
      if (!constrained || available >= relation.minResponsesPerWindow) {
        viableSources |= sourceBit;
      }
      if (sourceAnchors & zone & sourceBit) {
        ++mandatoryWindows;
        if (available < relation.minResponsesPerWindow) return false;
      }
    }

    if (!enoughPerBar(viableSources, bars, source->structuralMin)) {
      return false;
    }
    if (mandatoryWindows * relation.minResponsesPerWindow >
        target->structuralMax * bars) {
      return false;
    }
  }

  return true;
}

bool isTransformFunction(BarFunction function) {
  return function == BarFunction::Reduction ||
         function == BarFunction::Turnaround ||
         function == BarFunction::Break;
}

bool functionMatchesIntent(BarFunction function, TransformationIntent intent) {
  return (function == BarFunction::Reduction &&
          intent == TransformationIntent::Reduce) ||
         (function == BarFunction::Turnaround &&
          intent == TransformationIntent::Turnaround) ||
         (function == BarFunction::Break &&
          intent == TransformationIntent::Break);
}

const BarTrajectory* trajectoryFor(const RhythmCatalogView& catalog,
                                   TrajectoryId id) {
  for (uint8_t i = 0; i < catalog.trajectoryCount; ++i) {
    if (catalog.trajectories[i].id == id) return &catalog.trajectories[i];
  }
  return nullptr;
}

CatalogValidationResult validateTrajectories(const RhythmCatalogView& catalog) {
  if (catalog.trajectoryCount && !catalog.trajectories) {
    return fail(CatalogValidationError::MissingTrajectoryArray);
  }
  for (uint8_t i = 0; i < catalog.trajectoryCount; ++i) {
    const BarTrajectory& trajectory = catalog.trajectories[i];
    if (trajectory.id == kNoTrajectoryId) {
      return fail(CatalogValidationError::InvalidTrajectoryId,
                  kNoArchetypeIndex,
                  i);
    }
    for (uint8_t previous = 0; previous < i; ++previous) {
      if (catalog.trajectories[previous].id == trajectory.id) {
        return fail(CatalogValidationError::DuplicateTrajectoryId,
                    kNoArchetypeIndex,
                    i);
      }
    }
    if (!trajectory.barCount || trajectory.barCount > kMaxPhraseBars) {
      return fail(CatalogValidationError::InvalidTrajectoryBarCount,
                  kNoArchetypeIndex,
                  i);
    }
    for (uint8_t bar = 0; bar < trajectory.barCount; ++bar) {
      if (!validEnum(trajectory.bars[bar], BarFunction::Count)) {
        return fail(CatalogValidationError::InvalidTrajectoryBarFunction,
                    kNoArchetypeIndex,
                    i);
      }
    }
  }
  return {};
}

CatalogValidationResult validateLanes(const RhythmArchetype& archetype,
                                      uint16_t archetypeIndex) {
  if (!archetype.laneCount || archetype.laneCount > kMaxLanes) {
    return fail(CatalogValidationError::InvalidLaneCount, archetypeIndex);
  }
  if (!archetype.lanes) {
    return fail(CatalogValidationError::MissingLaneArray, archetypeIndex);
  }

  RhythmRoleMask seen = 0;
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    const LaneGrammar& lane = archetype.lanes[i];
    if (!validEnum(lane.role, RhythmRole::Count)) {
      return fail(CatalogValidationError::InvalidLaneRole, archetypeIndex, i);
    }
    const RhythmRoleMask roleBit = rhythmRoleBit(lane.role);
    if (seen & roleBit) {
      return fail(CatalogValidationError::DuplicateLaneRole,
                  archetypeIndex,
                  i);
    }
    seen = static_cast<RhythmRoleMask>(seen | roleBit);
    if (!(archetype.activeRoles & roleBit)) {
      return fail(CatalogValidationError::LaneRoleNotActive,
                  archetypeIndex,
                  i);
    }

    const StepMask zones[] = {
        lane.immutableAnchors,
        lane.canonicalAnchors,
        lane.preferred,
        lane.optional,
        lane.forbidden,
    };
    for (size_t first = 0; first < 5; ++first) {
      for (size_t second = first + 1; second < 5; ++second) {
        if (zones[first] & zones[second]) {
          return fail(CatalogValidationError::OverlappingLaneZones,
                      archetypeIndex,
                      i);
        }
      }
    }

    const StepMask gateMasks[] = {
        lane.shortGate, lane.heldGate, lane.tieGate};
    for (size_t first = 0; first < 3; ++first) {
      for (size_t second = first + 1; second < 3; ++second) {
        if (gateMasks[first] & gateMasks[second]) {
          return fail(CatalogValidationError::InvalidLaneGateMasks,
                      archetypeIndex,
                      i);
        }
      }
    }
    const StepMask declaredOnsets = static_cast<StepMask>(
        lane.immutableAnchors | lane.canonicalAnchors |
        lane.preferred | lane.optional);
    const StepMask explicitGates = static_cast<StepMask>(
        lane.shortGate | lane.heldGate | lane.tieGate);
    if ((explicitGates & ~declaredOnsets) ||
        (explicitGates & lane.forbidden)) {
      return fail(CatalogValidationError::InvalidLaneGateMasks,
                  archetypeIndex,
                  i);
    }

    if (lane.structuralMin > lane.structuralMax ||
        lane.structuralMax > kStepsPerBar ||
        lane.ornamentMax > kStepsPerBar) {
      return fail(CatalogValidationError::InvalidLaneDensity,
                  archetypeIndex,
                  i);
    }
    if (bitCount16(anchors(lane)) > lane.structuralMax) {
      return fail(CatalogValidationError::TooManyStructuralAnchors,
                  archetypeIndex,
                  i);
    }
  }

  return seen == archetype.activeRoles
             ? CatalogValidationResult{}
             : fail(CatalogValidationError::ActiveRoleMissingLane,
                    archetypeIndex);
}

CatalogValidationResult validateProtectedSpaces(
    const RhythmArchetype& archetype,
    uint16_t archetypeIndex) {
  if (archetype.protectedSpaceCount > kMaxProtectedSpaces) {
    return fail(CatalogValidationError::TooManyProtectedSpaces,
                archetypeIndex);
  }
  if (archetype.protectedSpaceCount && !archetype.protectedSpaces) {
    return fail(CatalogValidationError::MissingProtectedSpaceArray,
                archetypeIndex);
  }

  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    const ProtectedSpace& space = archetype.protectedSpaces[i];
    if (!space.steps || !space.affectedRoles) {
      return fail(CatalogValidationError::EmptyProtectedSpace,
                  archetypeIndex,
                  i);
    }
    if ((space.affectedRoles & ~kAllRhythmRoles) ||
        (space.affectedRoles & ~archetype.activeRoles)) {
      return fail(CatalogValidationError::InvalidProtectedSpaceRoles,
                  archetypeIndex,
                  i);
    }
    for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      if (!(space.affectedRoles & rhythmRoleBit(role))) continue;
      const LaneGrammar* lane = laneFor(archetype, role);
      if (!lane) {
        return fail(CatalogValidationError::ActiveRoleMissingLane,
                    archetypeIndex,
                    i);
      }
      if (space.steps & anchors(*lane)) {
        return fail(CatalogValidationError::ProtectedSpaceAnchorConflict,
                    archetypeIndex,
                    i);
      }
    }
  }
  return {};
}

CatalogValidationResult validateRelationshipShape(
    const RhythmArchetype& archetype,
    uint16_t archetypeIndex,
    uint8_t relationshipIndex) {
  const LaneRelationship& relation =
      archetype.relationships[relationshipIndex];

  if (!validEnum(relation.source, RhythmRole::Count) ||
      !validEnum(relation.target, RhythmRole::Count)) {
    return fail(CatalogValidationError::InvalidRelationshipRole,
                archetypeIndex,
                relationshipIndex);
  }
  if (!(archetype.activeRoles & rhythmRoleBit(relation.source)) ||
      !(archetype.activeRoles & rhythmRoleBit(relation.target))) {
    return fail(CatalogValidationError::RelationshipRoleNotActive,
                archetypeIndex,
                relationshipIndex);
  }
  if (relation.source == relation.target) {
    return fail(CatalogValidationError::SameRoleRelationship,
                archetypeIndex,
                relationshipIndex);
  }
  if (!validEnum(relation.op, RelationshipOp::Count)) {
    return fail(CatalogValidationError::InvalidRelationshipOp,
                archetypeIndex,
                relationshipIndex);
  }
  if (!validEnum(relation.strength, ConstraintStrength::Count)) {
    return fail(CatalogValidationError::InvalidConstraintStrength,
                archetypeIndex,
                relationshipIndex);
  }
  if (!validEnum(relation.scope, RelationshipScope::Count)) {
    return fail(CatalogValidationError::InvalidRelationshipScope,
                archetypeIndex,
                relationshipIndex);
  }
  if (!relation.zoneMask) {
    return fail(CatalogValidationError::EmptyRelationshipZone,
                archetypeIndex,
                relationshipIndex);
  }
  if ((relation.strength == ConstraintStrength::Soft) !=
      (relation.weight != 0)) {
    return fail(CatalogValidationError::InvalidRelationshipWeight,
                archetypeIndex,
                relationshipIndex);
  }

  const bool offsetsValid =
      relation.minOffset <= relation.maxOffset &&
      relation.minOffset >= -kMaxPhraseOffset &&
      relation.maxOffset <= kMaxPhraseOffset &&
      (relation.scope == RelationshipScope::Phrase ||
       (relation.minOffset >= -(kStepsPerBar - 1) &&
        relation.maxOffset <= (kStepsPerBar - 1)));
  const bool noCoincideCardinality =
      !relation.minMatches && !relation.maxMatches;
  const bool noRespondCardinality =
      !relation.minResponsesPerWindow && !relation.maxResponsesPerWindow;

  switch (relation.op) {
    case RelationshipOp::Exclude:
      if (relation.minOffset || relation.maxOffset) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincideCardinality || !noRespondCardinality) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::Coincide:
      if (relation.minOffset || relation.maxOffset) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if ((relation.maxMatches &&
           relation.maxMatches < relation.minMatches) ||
          !noRespondCardinality ||
          (relation.strength == ConstraintStrength::Hard &&
           !relation.minMatches && !relation.maxMatches)) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::Offset:
      if (!offsetsValid) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincideCardinality || !noRespondCardinality) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::Respond:
      if (!offsetsValid) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincideCardinality ||
          (relation.maxResponsesPerWindow &&
           relation.maxResponsesPerWindow <
               relation.minResponsesPerWindow)) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::FillGaps:
      if (relation.strength == ConstraintStrength::Hard) {
        return fail(CatalogValidationError::HardFillGapsUnsupported,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (relation.minOffset || relation.maxOffset) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincideCardinality || !noRespondCardinality) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::Count:
      return fail(CatalogValidationError::InvalidRelationshipOp,
                  archetypeIndex,
                  relationshipIndex);
  }

  return {};
}

CatalogValidationResult validateRelationships(const RhythmArchetype& archetype,
                                              uint16_t archetypeIndex) {
  if (archetype.relationshipCount > kMaxRelationships) {
    return fail(CatalogValidationError::TooManyRelationships,
                archetypeIndex);
  }
  if (archetype.relationshipCount && !archetype.relationships) {
    return fail(CatalogValidationError::MissingRelationshipArray,
                archetypeIndex);
  }

  for (uint8_t i = 0; i < archetype.relationshipCount; ++i) {
    CatalogValidationResult result =
        validateRelationshipShape(archetype, archetypeIndex, i);
    if (!result) return result;

    const LaneRelationship& relation = archetype.relationships[i];
    if (relation.strength != ConstraintStrength::Hard ||
        relation.op == RelationshipOp::FillGaps) {
      continue;
    }
    for (uint8_t bars = 1; bars <= kMaxPhraseBars; ++bars) {
      if (!(archetype.allowedPhraseBars & phraseBarsBit(bars))) continue;
      if (!hardRelationshipFeasible(archetype, relation, bars)) {
        return fail(CatalogValidationError::ImpossibleHardRelationship,
                    archetypeIndex,
                    i);
      }
    }
  }
  return {};
}

CatalogValidationResult validateTransforms(const RhythmArchetype& archetype,
                                           uint16_t archetypeIndex) {
  if (archetype.anchorTransformRuleCount > kMaxAnchorTransformRules) {
    return fail(CatalogValidationError::TooManyAnchorTransformRules,
                archetypeIndex);
  }
  if (archetype.anchorTransformRuleCount &&
      !archetype.anchorTransformRules) {
    return fail(CatalogValidationError::MissingAnchorTransformArray,
                archetypeIndex);
  }

  for (uint8_t i = 0; i < archetype.anchorTransformRuleCount; ++i) {
    const AnchorTransformRule& rule = archetype.anchorTransformRules[i];
    if (!validEnum(rule.role, RhythmRole::Count)) {
      return fail(CatalogValidationError::InvalidAnchorTransformRole,
                  archetypeIndex,
                  i);
    }
    if (!(archetype.activeRoles & rhythmRoleBit(rule.role))) {
      return fail(CatalogValidationError::AnchorTransformRoleNotActive,
                  archetypeIndex,
                  i);
    }
    if (!validEnum(rule.barFunction, BarFunction::Count) ||
        !isTransformFunction(rule.barFunction)) {
      return fail(CatalogValidationError::InvalidAnchorTransformBarFunction,
                  archetypeIndex,
                  i);
    }
    if (!validEnum(rule.intent, TransformationIntent::Count) ||
        rule.intent == TransformationIntent::Auto ||
        !functionMatchesIntent(rule.barFunction, rule.intent)) {
      return fail(CatalogValidationError::InvalidAnchorTransformIntent,
                  archetypeIndex,
                  i);
    }
    const StepMask affected = static_cast<StepMask>(
        rule.suppressibleCanonical | rule.displaceableCanonical);
    if (!affected) {
      return fail(CatalogValidationError::EmptyAnchorTransformRule,
                  archetypeIndex,
                  i);
    }
    const LaneGrammar* lane = laneFor(archetype, rule.role);
    if (!lane || (affected & ~lane->canonicalAnchors)) {
      return fail(CatalogValidationError::AnchorTransformOutsideCanonical,
                  archetypeIndex,
                  i);
    }
  }
  return {};
}

CatalogValidationResult validateTimingDensityAndMutation(
    const RhythmArchetype& archetype,
    uint16_t archetypeIndex) {
  if (!validEnum(archetype.timing.compatibility,
                 TimingCompatibility::Count)) {
    return fail(CatalogValidationError::InvalidTimingCompatibility,
                archetypeIndex);
  }
  if ((archetype.timing.affectedRoles & ~kAllRhythmRoles) ||
      (archetype.timing.affectedRoles & ~archetype.activeRoles) ||
      (archetype.timing.sensitiveSteps &&
       !archetype.timing.affectedRoles) ||
      (archetype.timing.compatibility != TimingCompatibility::StraightOnly &&
       (!archetype.timing.sensitiveSteps ||
        !archetype.timing.affectedRoles))) {
    return fail(CatalogValidationError::InvalidTimingEligibilityRoles,
                archetypeIndex);
  }

  const DensityContract& density = archetype.density;
  if (density.structuralMin > density.structuralPreferred ||
      density.structuralPreferred > density.structuralMax ||
      density.structuralMax > kMaxEventsPerBar ||
      density.ornamentMax > kMaxEventsPerBar) {
    return fail(CatalogValidationError::InvalidDensityContract,
                archetypeIndex);
  }

  uint16_t aggregateMin = 0;
  uint16_t aggregateMax = 0;
  uint16_t aggregateAnchors = 0;
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    const LaneGrammar& lane = archetype.lanes[i];
    aggregateMin += lane.structuralMin;
    aggregateMax += lane.structuralMax;
    aggregateAnchors += bitCount16(anchors(lane));
    if (bitCount16(candidates(archetype, lane)) < lane.structuralMin) {
      return fail(CatalogValidationError::InvalidDensityContract,
                  archetypeIndex,
                  i);
    }
  }
  if (aggregateMin > density.structuralMax ||
      aggregateAnchors > density.structuralMax ||
      density.structuralMin > aggregateMax) {
    return fail(CatalogValidationError::InvalidDensityContract,
                archetypeIndex);
  }

  for (uint8_t level = 0;
       level < static_cast<uint8_t>(RealizationLevel::Count);
       ++level) {
    const MutationBudget& budget = archetype.mutation.level[level];
    if ((budget.flags & ~kAllMutationFlags) ||
        (budget.allowedIntents & ~kConcreteTransformationIntents) ||
        budget.maxAdds > kMaxEventsPerBar ||
        budget.maxDrops > kMaxEventsPerBar ||
        budget.maxDisplacements > kMaxEventsPerBar ||
        budget.maxAccentChanges > kMaxEventsPerBar) {
      return fail(CatalogValidationError::InvalidMutationPolicy,
                  archetypeIndex,
                  level);
    }

    if (level == static_cast<uint8_t>(RealizationLevel::P1Canonical)) {
      const uint16_t forbidden =
          AllowPreferredDrops |
          AllowOptionalDisplace |
          AllowReduction |
          AllowTurnaround |
          AllowBreak;
      if (budget.allowedIntents ||
          (budget.flags & forbidden) ||
          budget.maxDrops ||
          budget.maxDisplacements) {
        return fail(CatalogValidationError::InvalidMutationPolicy,
                    archetypeIndex,
                    level);
      }
    }

    if (level == static_cast<uint8_t>(RealizationLevel::P2Variation) &&
        ((budget.allowedIntents & ~kP2TransformationIntents) ||
         (budget.flags & (AllowTurnaround | AllowBreak)))) {
      return fail(CatalogValidationError::InvalidMutationPolicy,
                  archetypeIndex,
                  level);
    }

    const bool reduceIntent =
        budget.allowedIntents &
        transformationIntentBit(TransformationIntent::Reduce);
    const bool turnaroundIntent =
        budget.allowedIntents &
        transformationIntentBit(TransformationIntent::Turnaround);
    const bool breakIntent =
        budget.allowedIntents &
        transformationIntentBit(TransformationIntent::Break);
    if (reduceIntent != static_cast<bool>(budget.flags & AllowReduction) ||
        turnaroundIntent != static_cast<bool>(budget.flags & AllowTurnaround) ||
        breakIntent != static_cast<bool>(budget.flags & AllowBreak)) {
      return fail(CatalogValidationError::InvalidMutationPolicy,
                  archetypeIndex,
                  level);
    }
  }
  return {};
}

CatalogValidationResult validateTrajectoryRefs(const RhythmCatalogView& catalog,
                                               const RhythmArchetype& archetype,
                                               uint16_t archetypeIndex) {
  if (!archetype.trajectoryCount ||
      archetype.trajectoryCount > kMaxTrajectoryRefs) {
    return fail(archetype.trajectoryCount
                    ? CatalogValidationError::TooManyTrajectoryRefs
                    : CatalogValidationError::InvalidTrajectoryRef,
                archetypeIndex);
  }
  if (!archetype.trajectories) {
    return fail(CatalogValidationError::MissingTrajectoryRefArray,
                archetypeIndex);
  }

  RealizationLevelMask coverage[kMaxPhraseBars]{};
  for (uint8_t i = 0; i < archetype.trajectoryCount; ++i) {
    const TrajectoryRef& ref = archetype.trajectories[i];
    if (ref.id == kNoTrajectoryId) {
      return fail(CatalogValidationError::InvalidTrajectoryRef,
                  archetypeIndex,
                  i);
    }
    for (uint8_t previous = 0; previous < i; ++previous) {
      if (archetype.trajectories[previous].id == ref.id) {
        return fail(CatalogValidationError::DuplicateTrajectoryRef,
                    archetypeIndex,
                    i);
      }
    }

    const BarTrajectory* trajectory = trajectoryFor(catalog, ref.id);
    if (!trajectory) {
      return fail(CatalogValidationError::UnknownTrajectoryId,
                  archetypeIndex,
                  i);
    }
    if (!ref.weight) {
      return fail(CatalogValidationError::InvalidTrajectoryWeight,
                  archetypeIndex,
                  i);
    }
    if (!ref.allowedLevels ||
        (ref.allowedLevels & ~kAllRealizationLevels)) {
      return fail(CatalogValidationError::InvalidRealizationLevelMask,
                  archetypeIndex,
                  i);
    }
    if (!(archetype.allowedPhraseBars &
          phraseBarsBit(trajectory->barCount))) {
      return fail(CatalogValidationError::TrajectoryLengthNotAllowed,
                  archetypeIndex,
                  i);
    }

    coverage[trajectory->barCount - 1] =
        static_cast<RealizationLevelMask>(
            coverage[trajectory->barCount - 1] | ref.allowedLevels);

    bool hasReduction = false;
    bool hasTurnaround = false;
    bool hasBreak = false;
    for (uint8_t bar = 0; bar < trajectory->barCount; ++bar) {
      hasReduction |= trajectory->bars[bar] == BarFunction::Reduction;
      hasTurnaround |= trajectory->bars[bar] == BarFunction::Turnaround;
      hasBreak |= trajectory->bars[bar] == BarFunction::Break;
    }

    if ((ref.allowedLevels &
         realizationLevelBit(RealizationLevel::P1Canonical)) &&
        (hasReduction || hasTurnaround || hasBreak)) {
      return fail(CatalogValidationError::TrajectoryLevelConflict,
                  archetypeIndex,
                  i);
    }
    if ((ref.allowedLevels &
         realizationLevelBit(RealizationLevel::P2Variation)) &&
        (hasTurnaround || hasBreak)) {
      return fail(CatalogValidationError::TrajectoryLevelConflict,
                  archetypeIndex,
                  i);
    }

    for (uint8_t level = 0;
         level < static_cast<uint8_t>(RealizationLevel::Count);
         ++level) {
      const RealizationLevel realizationLevel =
          static_cast<RealizationLevel>(level);
      if (!(ref.allowedLevels & realizationLevelBit(realizationLevel))) {
        continue;
      }
      const MutationBudget& budget = archetype.mutation.level[level];
      if ((hasReduction &&
           (!(budget.flags & AllowReduction) ||
            !(budget.allowedIntents & transformationIntentBit(
                TransformationIntent::Reduce)))) ||
          (hasTurnaround &&
           (!(budget.flags & AllowTurnaround) ||
            !(budget.allowedIntents & transformationIntentBit(
                TransformationIntent::Turnaround)))) ||
          (hasBreak &&
           (!(budget.flags & AllowBreak) ||
            !(budget.allowedIntents & transformationIntentBit(
                TransformationIntent::Break))))) {
        return fail(CatalogValidationError::TrajectoryLevelConflict,
                    archetypeIndex,
                    i);
      }
    }
  }

  for (uint8_t bars = 1; bars <= kMaxPhraseBars; ++bars) {
    if ((archetype.allowedPhraseBars & phraseBarsBit(bars)) &&
        coverage[bars - 1] != kAllRealizationLevels) {
      return fail(CatalogValidationError::MissingTrajectoryCoverage,
                  archetypeIndex);
    }
  }
  return {};
}

CatalogValidationResult validateArchetype(const RhythmCatalogView& catalog,
                                          uint16_t archetypeIndex) {
  const RhythmArchetype& archetype = catalog.archetypes[archetypeIndex];
  if (archetype.id == kNoArchetypeId) {
    return fail(CatalogValidationError::InvalidArchetypeId,
                archetypeIndex);
  }
  if (!validEnum(archetype.family, RhythmFamily::Count)) {
    return fail(CatalogValidationError::InvalidFamily, archetypeIndex);
  }
  if (!archetype.allowedPhraseBars ||
      (archetype.allowedPhraseBars & ~kAllPhraseBars)) {
    return fail(CatalogValidationError::InvalidPhraseBarsMask,
                archetypeIndex);
  }
  if (!archetype.activeRoles ||
      (archetype.activeRoles & ~kAllRhythmRoles)) {
    return fail(CatalogValidationError::InvalidActiveRoleMask,
                archetypeIndex);
  }

  CatalogValidationResult result =
      validateLanes(archetype, archetypeIndex);
  if (!result) return result;
  result = validateProtectedSpaces(archetype, archetypeIndex);
  if (!result) return result;
  result = validateRelationships(archetype, archetypeIndex);
  if (!result) return result;
  result = validateTransforms(archetype, archetypeIndex);
  if (!result) return result;
  result = validateTimingDensityAndMutation(archetype, archetypeIndex);
  if (!result) return result;
  return validateTrajectoryRefs(catalog, archetype, archetypeIndex);
}

}  // namespace

CatalogValidationResult validateRhythmCatalog(const RhythmCatalogView& catalog) {
  if (!catalog.archetypeCount) {
    return fail(CatalogValidationError::EmptyArchetypeCatalog);
  }
  if (!catalog.archetypes) {
    return fail(CatalogValidationError::NullArchetypeArray);
  }

  CatalogValidationResult result = validateTrajectories(catalog);
  if (!result) return result;

  for (uint16_t i = 0; i < catalog.archetypeCount; ++i) {
    for (uint16_t previous = 0; previous < i; ++previous) {
      if (catalog.archetypes[previous].id == catalog.archetypes[i].id) {
        return fail(CatalogValidationError::DuplicateArchetypeId, i);
      }
    }
    result = validateArchetype(catalog, i);
    if (!result) return result;
  }
  return {};
}

}  // namespace GroovePuterRhythm
