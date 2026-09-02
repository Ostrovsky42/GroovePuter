#include "relationship_resolver.h"

#include <cstddef>

namespace GroovePuterRhythm {
namespace {

struct Coordinate {
  uint8_t bar = 0;
  uint8_t step = 0;
  uint8_t absolute = 0;
};

bool roleValid(RhythmRole role) {
  return static_cast<uint8_t>(role) < kRhythmRoleCount;
}

bool stepInZone(const LaneRelationship& relationship, uint8_t step) {
  return (relationship.zoneMask & stepBit(step)) != 0;
}

bool occupied(const PhraseOccupancy& occupancy,
              uint8_t bar,
              RhythmRole role,
              uint8_t step) {
  if (bar >= occupancy.barCount || !roleValid(role) || step >= kStepsPerBar) {
    return false;
  }
  return (occupancy.roleMasks[bar][static_cast<uint8_t>(role)] & stepBit(step)) != 0;
}

uint8_t absoluteStep(uint8_t bar, uint8_t step) {
  return static_cast<uint8_t>(bar * kStepsPerBar + step);
}

int absoluteDelta(const Coordinate& source, const Coordinate& target) {
  return static_cast<int>(target.absolute) - static_cast<int>(source.absolute);
}

int absoluteValue(int value) {
  return value < 0 ? -value : value;
}

bool sameScopeWindow(const LaneRelationship& relationship,
                     const Coordinate& source,
                     const Coordinate& target) {
  if (relationship.scope == RelationshipScope::BarLocal &&
      source.bar != target.bar) {
    return false;
  }
  return true;
}

bool sourceQualifies(const LaneRelationship& relationship,
                     const Coordinate& source) {
  return stepInZone(relationship, source.step);
}

bool targetQualifies(const LaneRelationship& relationship,
                     const Coordinate& target) {
  return stepInZone(relationship, target.step);
}

bool sourceIsInWindow(const LaneRelationship& relationship,
                      const Coordinate& source,
                      const Coordinate& target) {
  if (!sameScopeWindow(relationship, source, target) ||
      !sourceQualifies(relationship, source) ||
      !targetQualifies(relationship, target)) {
    return false;
  }
  const int delta = absoluteDelta(source, target);
  return delta >= relationship.minOffset && delta <= relationship.maxOffset;
}

uint8_t collectRoleCoordinates(const PhraseOccupancy& occupancy,
                               RhythmRole role,
                               const LaneRelationship& relationship,
                               Coordinate* out,
                               uint8_t capacity) {
  uint8_t count = 0;
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    const StepMask mask = occupancy.roleMasks[bar][static_cast<uint8_t>(role)];
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (!(mask & stepBit(step)) || !stepInZone(relationship, step)) continue;
      if (count >= capacity) return count;
      out[count].bar = bar;
      out[count].step = step;
      out[count].absolute = absoluteStep(bar, step);
      ++count;
    }
  }
  return count;
}

uint8_t countCoincidences(const LaneRelationship& relationship,
                          const PhraseOccupancy& occupancy) {
  uint8_t matches = 0;
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    const StepMask source = occupancy.roleMasks[bar][static_cast<uint8_t>(relationship.source)];
    const StepMask target = occupancy.roleMasks[bar][static_cast<uint8_t>(relationship.target)];
    StepMask overlap = static_cast<StepMask>(source & target & relationship.zoneMask);
    while (overlap) {
      overlap = static_cast<StepMask>(overlap & (overlap - 1u));
      ++matches;
    }
  }
  return matches;
}

bool excludeSatisfied(const LaneRelationship& relationship,
                     const PhraseOccupancy& occupancy) {
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    const StepMask source = occupancy.roleMasks[bar][static_cast<uint8_t>(relationship.source)];
    const StepMask target = occupancy.roleMasks[bar][static_cast<uint8_t>(relationship.target)];
    if ((source & target & relationship.zoneMask) != 0) return false;
  }
  return true;
}

bool coincideSatisfied(const LaneRelationship& relationship,
                       const PhraseOccupancy& occupancy) {
  const uint8_t matches = countCoincidences(relationship, occupancy);
  if (matches < relationship.minMatches) return false;
  if (relationship.maxMatches && matches > relationship.maxMatches) return false;
  return true;
}

bool offsetSatisfied(const LaneRelationship& relationship,
                     const PhraseOccupancy& occupancy) {
  Coordinate sources[kMaxPhraseBars * kStepsPerBar]{};
  Coordinate targets[kMaxPhraseBars * kStepsPerBar]{};
  const uint8_t sourceCount = collectRoleCoordinates(
      occupancy, relationship.source, relationship, sources,
      static_cast<uint8_t>(sizeof(sources) / sizeof(sources[0])));
  const uint8_t targetCount = collectRoleCoordinates(
      occupancy, relationship.target, relationship, targets,
      static_cast<uint8_t>(sizeof(targets) / sizeof(targets[0])));

  for (uint8_t targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
    bool supported = false;
    for (uint8_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) {
      if (sourceIsInWindow(relationship, sources[sourceIndex], targets[targetIndex])) {
        supported = true;
        break;
      }
    }
    if (!supported) return false;
  }
  return true;
}

bool respondMaximumsSatisfied(const LaneRelationship& relationship,
                              const PhraseOccupancy& occupancy,
                              bool requireMinimums) {
  Coordinate sources[kMaxPhraseBars * kStepsPerBar]{};
  Coordinate targets[kMaxPhraseBars * kStepsPerBar]{};
  uint8_t responseCount[kMaxPhraseBars * kStepsPerBar]{};

  const uint8_t sourceCount = collectRoleCoordinates(
      occupancy, relationship.source, relationship, sources,
      static_cast<uint8_t>(sizeof(sources) / sizeof(sources[0])));
  const uint8_t targetCount = collectRoleCoordinates(
      occupancy, relationship.target, relationship, targets,
      static_cast<uint8_t>(sizeof(targets) / sizeof(targets[0])));

  // Stable deterministic ownership: each target goes to the nearest eligible
  // source, then the earlier phrase coordinate wins ties.
  for (uint8_t targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
    int bestSource = -1;
    int bestDistance = 0x7FFF;
    for (uint8_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) {
      if (!sourceIsInWindow(relationship, sources[sourceIndex], targets[targetIndex])) {
        continue;
      }
      const int distance = absoluteValue(
          absoluteDelta(sources[sourceIndex], targets[targetIndex]));
      if (distance < bestDistance) {
        bestDistance = distance;
        bestSource = sourceIndex;
      }
    }
    if (bestSource >= 0) ++responseCount[bestSource];
  }

  for (uint8_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) {
    if (requireMinimums &&
        responseCount[sourceIndex] < relationship.minResponsesPerWindow) {
      return false;
    }
    if (relationship.maxResponsesPerWindow &&
        responseCount[sourceIndex] > relationship.maxResponsesPerWindow) {
      return false;
    }
  }
  return true;
}

bool respondSatisfied(const LaneRelationship& relationship,
                      const PhraseOccupancy& occupancy) {
  return respondMaximumsSatisfied(relationship, occupancy, true);
}

bool hardRelationshipSatisfied(const LaneRelationship& relationship,
                               const PhraseOccupancy& occupancy) {
  switch (relationship.op) {
    case RelationshipOp::Exclude:
      return excludeSatisfied(relationship, occupancy);
    case RelationshipOp::Coincide:
      return coincideSatisfied(relationship, occupancy);
    case RelationshipOp::Offset:
      return offsetSatisfied(relationship, occupancy);
    case RelationshipOp::Respond:
      return respondSatisfied(relationship, occupancy);
    case RelationshipOp::FillGaps:
      // Catalog validation rejects hard FillGaps in Core v1.
      return relationship.strength != ConstraintStrength::Hard;
    default:
      return false;
  }
}

bool candidateCreatesExcludeConflict(const LaneRelationship& relationship,
                                     const PhraseOccupancy& occupancy,
                                     uint8_t barIndex,
                                     RhythmRole role,
                                     uint8_t step) {
  if (!stepInZone(relationship, step)) return false;
  if (role == relationship.target) {
    return occupied(occupancy, barIndex, relationship.source, step);
  }
  if (role == relationship.source) {
    return occupied(occupancy, barIndex, relationship.target, step);
  }
  return false;
}

bool targetHasOffsetSource(const LaneRelationship& relationship,
                           const PhraseOccupancy& occupancy,
                           uint8_t barIndex,
                           uint8_t step) {
  Coordinate target{barIndex, step, absoluteStep(barIndex, step)};
  for (uint8_t sourceBar = 0; sourceBar < occupancy.barCount; ++sourceBar) {
    for (uint8_t sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
      if (!occupied(occupancy, sourceBar, relationship.source, sourceStep)) continue;
      Coordinate source{sourceBar, sourceStep, absoluteStep(sourceBar, sourceStep)};
      if (sourceIsInWindow(relationship, source, target)) return true;
    }
  }
  return false;
}

bool sourceAtCandidateWindow(const LaneRelationship& relationship,
                             const PhraseOccupancy& occupancy,
                             uint8_t barIndex,
                             uint8_t step) {
  return targetHasOffsetSource(relationship, occupancy, barIndex, step);
}

int16_t weighted(bool preferred, uint8_t weight) {
  return preferred ? static_cast<int16_t>(weight)
                   : static_cast<int16_t>(-static_cast<int16_t>(weight));
}

}  // namespace

bool relationshipSatisfied(const LaneRelationship& relationship,
                           const PhraseOccupancy& occupancy) {
  if (occupancy.barCount == 0 || occupancy.barCount > kMaxPhraseBars ||
      !roleValid(relationship.source) || !roleValid(relationship.target)) {
    return false;
  }
  return hardRelationshipSatisfied(relationship, occupancy);
}

bool hardRelationshipsSatisfied(const RhythmArchetype& archetype,
                                const PhraseOccupancy& occupancy) {
  for (uint8_t i = 0; i < archetype.relationshipCount; ++i) {
    const LaneRelationship& relationship = archetype.relationships[i];
    if (relationship.strength != ConstraintStrength::Hard) continue;
    if (!hardRelationshipSatisfied(relationship, occupancy)) return false;
  }
  return true;
}

bool hardCandidateAdditionAllowed(const RhythmArchetype& archetype,
                                  const PhraseOccupancy& occupancy,
                                  uint8_t barIndex,
                                  RhythmRole role,
                                  uint8_t step) {
  if (barIndex >= occupancy.barCount || step >= kStepsPerBar || !roleValid(role)) {
    return false;
  }

  PhraseOccupancy candidate = occupancy;
  candidate.roleMasks[barIndex][static_cast<uint8_t>(role)] =
      static_cast<StepMask>(candidate.roleMasks[barIndex][static_cast<uint8_t>(role)] |
                            stepBit(step));

  for (uint8_t i = 0; i < archetype.relationshipCount; ++i) {
    const LaneRelationship& relationship = archetype.relationships[i];
    if (relationship.strength != ConstraintStrength::Hard) continue;

    if (relationship.op == RelationshipOp::Exclude &&
        candidateCreatesExcludeConflict(relationship, occupancy, barIndex, role, step)) {
      return false;
    }

    if (relationship.op == RelationshipOp::Offset &&
        role == relationship.target && stepInZone(relationship, step) &&
        !targetHasOffsetSource(relationship, occupancy, barIndex, step)) {
      return false;
    }

    if (relationship.op == RelationshipOp::Coincide && relationship.maxMatches &&
        countCoincidences(relationship, candidate) > relationship.maxMatches) {
      return false;
    }

    if (relationship.op == RelationshipOp::Respond &&
        role == relationship.target &&
        !respondMaximumsSatisfied(relationship, candidate, false)) {
      return false;
    }
  }

  return true;
}

int16_t softRelationshipCandidateScore(const RhythmArchetype& archetype,
                                       const PhraseOccupancy& occupancy,
                                       uint8_t barIndex,
                                       RhythmRole role,
                                       uint8_t step) {
  if (barIndex >= occupancy.barCount || step >= kStepsPerBar || !roleValid(role)) {
    return 0;
  }

  int16_t score = 0;
  for (uint8_t i = 0; i < archetype.relationshipCount; ++i) {
    const LaneRelationship& relationship = archetype.relationships[i];
    if (relationship.strength != ConstraintStrength::Soft ||
        role != relationship.target || !stepInZone(relationship, step)) {
      continue;
    }

    switch (relationship.op) {
      case RelationshipOp::Exclude:
        score = static_cast<int16_t>(score + weighted(
            !occupied(occupancy, barIndex, relationship.source, step),
            relationship.weight));
        break;
      case RelationshipOp::Coincide:
        score = static_cast<int16_t>(score + weighted(
            occupied(occupancy, barIndex, relationship.source, step),
            relationship.weight));
        break;
      case RelationshipOp::Offset:
      case RelationshipOp::Respond:
        score = static_cast<int16_t>(score + weighted(
            sourceAtCandidateWindow(relationship, occupancy, barIndex, step),
            relationship.weight));
        break;
      case RelationshipOp::FillGaps:
        score = static_cast<int16_t>(score + weighted(
            !occupied(occupancy, barIndex, relationship.source, step),
            relationship.weight));
        break;
      default:
        break;
    }
  }
  return score;
}

}  // namespace GroovePuterRhythm
