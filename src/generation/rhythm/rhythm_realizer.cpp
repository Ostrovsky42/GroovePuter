#include "rhythm_realizer.h"

#include <cstddef>

namespace GroovePuterRhythm {
namespace {

uint8_t bitCount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

const RhythmArchetype* archetypeFor(const RhythmCatalogView& catalog,
                                    RhythmArchetypeId id) {
  for (uint16_t i = 0; i < catalog.archetypeCount; ++i) {
    if (catalog.archetypes[i].id == id) return &catalog.archetypes[i];
  }
  return nullptr;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    if (archetype.lanes[i].role == role) return &archetype.lanes[i];
  }
  return nullptr;
}

StepMask anchorMask(const LaneGrammar& lane) {
  return static_cast<StepMask>(lane.immutableAnchors |
                               lane.canonicalAnchors);
}

StepMask protectedMask(const RhythmArchetype& archetype, RhythmRole role) {
  StepMask mask = 0;
  const RhythmRoleMask roleMask = rhythmRoleBit(role);
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    if (archetype.protectedSpaces[i].affectedRoles & roleMask) {
      mask = static_cast<StepMask>(mask |
                                   archetype.protectedSpaces[i].steps);
    }
  }
  return mask;
}

StepMask structuralLegalMask(const RhythmArchetype& archetype,
                             const LaneGrammar& lane) {
  const StepMask declared = static_cast<StepMask>(
      lane.immutableAnchors | lane.canonicalAnchors |
      lane.preferred | lane.optional);
  return static_cast<StepMask>(declared & ~lane.forbidden &
                               ~protectedMask(archetype, lane.role));
}

bool isOnsetLegal(const RhythmArchetype& archetype,
                  const LaneGrammar& lane,
                  uint8_t step) {
  return step < kStepsPerBar &&
         (structuralLegalMask(archetype, lane) & stepBit(step));
}

uint8_t structuralCount(const PhraseOccupancy& occupancy,
                        uint8_t bar,
                        RhythmRole role) {
  return bitCount16(
      occupancy.roleMasks[bar][static_cast<uint8_t>(role)]);
}

uint16_t totalStructural(const PhraseOccupancy& occupancy, uint8_t bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(occupancy.roleMasks[bar][role]);
  }
  return total;
}

uint16_t totalOrnaments(const RhythmPhrasePlan& plan, uint8_t bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(plan.bars[bar].roles[role].ghosts);
  }
  return total;
}

uint32_t candidateCoordinate(uint8_t bar, RhythmRole role, uint8_t step) {
  return (static_cast<uint32_t>(bar) << 16u) |
         (static_cast<uint32_t>(static_cast<uint8_t>(role)) << 8u) |
         step;
}

int32_t candidateScore(const RhythmArchetype& archetype,
                       const LaneGrammar& lane,
                       const PhraseOccupancy& occupancy,
                       uint8_t bar,
                       uint8_t step,
                       uint32_t seed) {
  const StepMask bit = stepBit(step);
  int32_t score = 0;
  if (lane.preferred & bit) score += 4096;
  if (lane.optional & bit) score += 1024;
  score += static_cast<int32_t>(softRelationshipCandidateScore(
      archetype, occupancy, bar, lane.role, step)) * 8;
  score += static_cast<int32_t>(
      deterministicValue(seed,
                         candidateCoordinate(bar, lane.role, step)) & 0xFFu);
  return score;
}

bool addStructuralCandidate(const RhythmArchetype& archetype,
                            const LaneGrammar& lane,
                            PhraseOccupancy& occupancy,
                            uint8_t bar,
                            uint8_t step) {
  if (!isOnsetLegal(archetype, lane, step)) return false;
  StepMask& mask =
      occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)];
  const StepMask bit = stepBit(step);
  if (mask & bit) return true;
  if (bitCount16(mask) >= lane.structuralMax ||
      totalStructural(occupancy, bar) >= archetype.density.structuralMax ||
      !hardCandidateAdditionAllowed(archetype, occupancy, bar,
                                    lane.role, step)) {
    return false;
  }
  mask = static_cast<StepMask>(mask | bit);
  return true;
}

bool addBestStructuralCandidate(const RhythmArchetype& archetype,
                                const LaneGrammar& lane,
                                PhraseOccupancy& occupancy,
                                uint8_t bar,
                                uint32_t seed) {
  const StepMask current =
      occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)];
  const StepMask candidates = static_cast<StepMask>(
      structuralLegalMask(archetype, lane) & ~current);
  int bestStep = -1;
  int32_t bestScore = -0x7FFFFFFF;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (!(candidates & stepBit(step))) continue;
    if (!hardCandidateAdditionAllowed(archetype, occupancy, bar,
                                      lane.role, step)) {
      continue;
    }
    const int32_t score = candidateScore(
        archetype, lane, occupancy, bar, step, seed);
    if (bestStep < 0 || score > bestScore) {
      bestStep = step;
      bestScore = score;
    }
  }
  if (bestStep < 0) return false;
  return addStructuralCandidate(archetype, lane, occupancy, bar,
                                static_cast<uint8_t>(bestStep));
}

bool canDropIdentityEvent(const LaneGrammar& lane, uint8_t step) {
  return (anchorMask(lane) & stepBit(step)) == 0;
}

bool dropIdentityEvent(const RhythmArchetype& archetype,
                       RhythmRole role,
                       PhraseOccupancy& occupancy,
                       uint8_t bar,
                       uint8_t step) {
  const LaneGrammar* lane = laneFor(archetype, role);
  if (!lane || !canDropIdentityEvent(*lane, step)) return false;
  StepMask& mask =
      occupancy.roleMasks[bar][static_cast<uint8_t>(role)];
  const StepMask bit = stepBit(step);
  if (!(mask & bit)) return true;
  if (bitCount16(mask) <= lane->structuralMin) return false;
  mask = static_cast<StepMask>(mask & ~bit);
  return true;
}

bool coordinateInPhrase(uint8_t barCount, int absolute) {
  return absolute >= 0 &&
         absolute < static_cast<int>(barCount * kStepsPerBar);
}

bool relationCoordinateAllowed(const LaneRelationship& relation,
                               uint8_t sourceBar,
                               uint8_t sourceStep,
                               uint8_t targetBar,
                               uint8_t targetStep) {
  if (!(relation.zoneMask & stepBit(sourceStep)) ||
      !(relation.zoneMask & stepBit(targetStep))) {
    return false;
  }
  if (relation.scope == RelationshipScope::BarLocal &&
      sourceBar != targetBar) {
    return false;
  }
  const int sourceAbsolute = sourceBar * kStepsPerBar + sourceStep;
  const int targetAbsolute = targetBar * kStepsPerBar + targetStep;
  const int delta = targetAbsolute - sourceAbsolute;
  return delta >= relation.minOffset && delta <= relation.maxOffset;
}

bool offsetTargetSupported(const LaneRelationship& relation,
                           const PhraseOccupancy& occupancy,
                           uint8_t targetBar,
                           uint8_t targetStep) {
  if (!(relation.zoneMask & stepBit(targetStep))) return true;
  for (uint8_t sourceBar = 0; sourceBar < occupancy.barCount; ++sourceBar) {
    const StepMask sourceMask = occupancy.roleMasks[
        sourceBar][static_cast<uint8_t>(relation.source)];
    for (uint8_t sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
      if (!(sourceMask & stepBit(sourceStep))) continue;
      if (relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                    targetBar, targetStep)) {
        return true;
      }
    }
  }
  return false;
}

uint8_t coincidenceCount(const LaneRelationship& relation,
                         const PhraseOccupancy& occupancy) {
  uint8_t count = 0;
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    StepMask overlap = static_cast<StepMask>(
        occupancy.roleMasks[bar][static_cast<uint8_t>(relation.source)] &
        occupancy.roleMasks[bar][static_cast<uint8_t>(relation.target)] &
        relation.zoneMask);
    while (overlap) {
      overlap = static_cast<StepMask>(overlap & (overlap - 1u));
      ++count;
    }
  }
  return count;
}

uint16_t respondDeficit(const LaneRelationship& relation,
                        const PhraseOccupancy& occupancy) {
  constexpr uint8_t kCoordinateCount = kMaxPhraseBars * kStepsPerBar;
  uint8_t responseCount[kCoordinateCount]{};

  for (uint8_t targetBar = 0; targetBar < occupancy.barCount; ++targetBar) {
    const StepMask targetMask = occupancy.roleMasks[
        targetBar][static_cast<uint8_t>(relation.target)];
    for (uint8_t targetStep = 0; targetStep < kStepsPerBar; ++targetStep) {
      if (!(targetMask & stepBit(targetStep)) ||
          !(relation.zoneMask & stepBit(targetStep))) {
        continue;
      }

      int bestSourceAbsolute = -1;
      int bestDistance = 0x7FFF;
      const int targetAbsolute = targetBar * kStepsPerBar + targetStep;
      for (uint8_t sourceBar = 0; sourceBar < occupancy.barCount; ++sourceBar) {
        const StepMask sourceMask = occupancy.roleMasks[
            sourceBar][static_cast<uint8_t>(relation.source)];
        for (uint8_t sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
          if (!(sourceMask & stepBit(sourceStep)) ||
              !relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                         targetBar, targetStep)) {
            continue;
          }
          const int sourceAbsolute = sourceBar * kStepsPerBar + sourceStep;
          const int delta = targetAbsolute - sourceAbsolute;
          const int distance = delta < 0 ? -delta : delta;
          if (distance < bestDistance) {
            bestDistance = distance;
            bestSourceAbsolute = sourceAbsolute;
          }
        }
      }
      if (bestSourceAbsolute >= 0) {
        ++responseCount[static_cast<uint8_t>(bestSourceAbsolute)];
      }
    }
  }

  uint16_t deficit = 0;
  for (uint8_t sourceBar = 0; sourceBar < occupancy.barCount; ++sourceBar) {
    const StepMask sourceMask = occupancy.roleMasks[
        sourceBar][static_cast<uint8_t>(relation.source)];
    for (uint8_t sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
      if (!(sourceMask & stepBit(sourceStep)) ||
          !(relation.zoneMask & stepBit(sourceStep))) {
        continue;
      }
      const uint8_t absolute = static_cast<uint8_t>(
          sourceBar * kStepsPerBar + sourceStep);
      const uint8_t responses = responseCount[absolute];
      if (responses < relation.minResponsesPerWindow) {
        deficit = static_cast<uint16_t>(
            deficit + relation.minResponsesPerWindow - responses);
      }
      if (relation.maxResponsesPerWindow &&
          responses > relation.maxResponsesPerWindow) {
        deficit = static_cast<uint16_t>(
            deficit + responses - relation.maxResponsesPerWindow);
      }
    }
  }
  return deficit;
}

bool repairExclude(const RhythmArchetype& archetype,
                   const LaneRelationship& relation,
                   PhraseOccupancy& occupancy,
                   uint32_t seed) {
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    StepMask conflicts = static_cast<StepMask>(
        occupancy.roleMasks[bar][static_cast<uint8_t>(relation.source)] &
        occupancy.roleMasks[bar][static_cast<uint8_t>(relation.target)] &
        relation.zoneMask);
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (!(conflicts & stepBit(step))) continue;
      const bool targetFirst =
          (deterministicValue(
               seed, candidateCoordinate(bar, relation.target, step)) & 1u) != 0;
      if (targetFirst) {
        if (dropIdentityEvent(archetype, relation.target,
                              occupancy, bar, step)) {
          continue;
        }
        if (dropIdentityEvent(archetype, relation.source,
                              occupancy, bar, step)) {
          continue;
        }
      } else {
        if (dropIdentityEvent(archetype, relation.source,
                              occupancy, bar, step)) {
          continue;
        }
        if (dropIdentityEvent(archetype, relation.target,
                              occupancy, bar, step)) {
          continue;
        }
      }
      return false;
    }
  }
  return true;
}

bool repairCoincide(const RhythmArchetype& archetype,
                    const LaneRelationship& relation,
                    PhraseOccupancy& occupancy,
                    uint32_t seed) {
  const LaneGrammar* sourceLane = laneFor(archetype, relation.source);
  const LaneGrammar* targetLane = laneFor(archetype, relation.target);
  if (!sourceLane || !targetLane) return false;

  uint8_t matches = coincidenceCount(relation, occupancy);
  while (relation.maxMatches && matches > relation.maxMatches) {
    bool removed = false;
    for (uint8_t bar = 0; bar < occupancy.barCount && !removed; ++bar) {
      StepMask overlap = static_cast<StepMask>(
          occupancy.roleMasks[bar][static_cast<uint8_t>(relation.source)] &
          occupancy.roleMasks[bar][static_cast<uint8_t>(relation.target)] &
          relation.zoneMask);
      for (uint8_t step = 0; step < kStepsPerBar; ++step) {
        if (!(overlap & stepBit(step))) continue;
        if (dropIdentityEvent(archetype, relation.target,
                              occupancy, bar, step) ||
            dropIdentityEvent(archetype, relation.source,
                              occupancy, bar, step)) {
          removed = true;
          break;
        }
      }
    }
    if (!removed) return false;
    matches = coincidenceCount(relation, occupancy);
  }

  uint8_t guard = 0;
  while (matches < relation.minMatches &&
         guard++ < kMaxPhraseBars * kStepsPerBar) {
    bool added = false;

    for (uint8_t bar = 0; bar < occupancy.barCount && !added; ++bar) {
      const StepMask sourceMask = occupancy.roleMasks[
          bar][static_cast<uint8_t>(relation.source)];
      for (uint8_t step = 0; step < kStepsPerBar; ++step) {
        const StepMask bit = stepBit(step);
        if (!(relation.zoneMask & bit) || !(sourceMask & bit) ||
            (occupancy.roleMasks[bar][static_cast<uint8_t>(relation.target)] &
             bit)) {
          continue;
        }
        if (addStructuralCandidate(archetype, *targetLane,
                                   occupancy, bar, step)) {
          added = true;
          break;
        }
      }
    }

    if (!added) {
      for (uint8_t bar = 0; bar < occupancy.barCount && !added; ++bar) {
        const StepMask targetMask = occupancy.roleMasks[
            bar][static_cast<uint8_t>(relation.target)];
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          const StepMask bit = stepBit(step);
          if (!(relation.zoneMask & bit) || !(targetMask & bit) ||
              (occupancy.roleMasks[bar][static_cast<uint8_t>(relation.source)] &
               bit)) {
            continue;
          }
          if (addStructuralCandidate(archetype, *sourceLane,
                                     occupancy, bar, step)) {
            added = true;
            break;
          }
        }
      }
    }

    if (!added) {
      bool foundPair = false;
      uint32_t bestRank = 0;
      PhraseOccupancy bestOccupancy{};
      for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          const StepMask bit = stepBit(step);
          if (!(relation.zoneMask & bit) ||
              !isOnsetLegal(archetype, *sourceLane, step) ||
              !isOnsetLegal(archetype, *targetLane, step)) {
            continue;
          }
          PhraseOccupancy trial = occupancy;
          if (!addStructuralCandidate(archetype, *sourceLane,
                                      trial, bar, step) ||
              !addStructuralCandidate(archetype, *targetLane,
                                      trial, bar, step)) {
            continue;
          }
          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(bar, relation.target, step));
          if (!foundPair || rank > bestRank) {
            foundPair = true;
            bestRank = rank;
            bestOccupancy = trial;
          }
        }
      }
      if (foundPair) {
        occupancy = bestOccupancy;
        added = true;
      }
    }

    if (!added) return false;
    matches = coincidenceCount(relation, occupancy);
  }
  return matches >= relation.minMatches &&
         (!relation.maxMatches || matches <= relation.maxMatches);
}

bool repairOffset(const RhythmArchetype& archetype,
                  const LaneRelationship& relation,
                  PhraseOccupancy& occupancy,
                  uint32_t seed) {
  const LaneGrammar* sourceLane = laneFor(archetype, relation.source);
  if (!sourceLane) return false;

  for (uint8_t targetBar = 0; targetBar < occupancy.barCount; ++targetBar) {
    StepMask targetMask = occupancy.roleMasks[
        targetBar][static_cast<uint8_t>(relation.target)];
    for (uint8_t targetStep = 0; targetStep < kStepsPerBar; ++targetStep) {
      const StepMask bit = stepBit(targetStep);
      if (!(targetMask & bit) || !(relation.zoneMask & bit) ||
          offsetTargetSupported(relation, occupancy,
                                targetBar, targetStep)) {
        continue;
      }

      bool foundSource = false;
      uint32_t bestRank = 0;
      PhraseOccupancy bestOccupancy{};
      const int targetAbsolute =
          targetBar * kStepsPerBar + targetStep;
      for (int offset = relation.minOffset;
           offset <= relation.maxOffset;
           ++offset) {
        const int sourceAbsolute = targetAbsolute - offset;
        if (!coordinateInPhrase(occupancy.barCount, sourceAbsolute)) continue;
        const uint8_t sourceBar = static_cast<uint8_t>(
            sourceAbsolute / kStepsPerBar);
        const uint8_t sourceStep = static_cast<uint8_t>(
            sourceAbsolute % kStepsPerBar);
        if (!relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                       targetBar, targetStep) ||
            !isOnsetLegal(archetype, *sourceLane, sourceStep)) {
          continue;
        }

        PhraseOccupancy trial = occupancy;
        if (!addStructuralCandidate(archetype, *sourceLane, trial,
                                    sourceBar, sourceStep)) {
          continue;
        }
        const uint32_t rank = deterministicValue(
            seed, candidateCoordinate(sourceBar,
                                      relation.source, sourceStep));
        if (!foundSource || rank > bestRank) {
          foundSource = true;
          bestRank = rank;
          bestOccupancy = trial;
        }
      }

      bool repaired = false;
      if (foundSource) {
        occupancy = bestOccupancy;
        repaired = true;
      }
      if (!repaired) {
        repaired = dropIdentityEvent(archetype, relation.target,
                                     occupancy, targetBar, targetStep);
      }
      if (!repaired) return false;
      targetMask = occupancy.roleMasks[
          targetBar][static_cast<uint8_t>(relation.target)];
    }
  }
  return relationshipSatisfied(relation, occupancy);
}

bool repairRespond(const RhythmArchetype& archetype,
                   const LaneRelationship& relation,
                   PhraseOccupancy& occupancy,
                   uint32_t seed) {
  const LaneGrammar* targetLane = laneFor(archetype, relation.target);
  if (!targetLane) return false;
  if (relationshipSatisfied(relation, occupancy)) return true;

  uint8_t guard = 0;
  while (!relationshipSatisfied(relation, occupancy) &&
         guard++ < kMaxPhraseBars * kStepsPerBar) {
    const uint16_t currentDeficit = respondDeficit(relation, occupancy);
    int bestBar = -1;
    int bestStep = -1;
    uint16_t bestDeficit = currentDeficit;
    uint32_t bestRank = 0;
    PhraseOccupancy bestOccupancy{};

    for (uint8_t sourceBar = 0;
         sourceBar < occupancy.barCount;
         ++sourceBar) {
      const StepMask sourceMask = occupancy.roleMasks[
          sourceBar][static_cast<uint8_t>(relation.source)];
      for (uint8_t sourceStep = 0;
           sourceStep < kStepsPerBar;
           ++sourceStep) {
        if (!(sourceMask & stepBit(sourceStep)) ||
            !(relation.zoneMask & stepBit(sourceStep))) {
          continue;
        }
        const int sourceAbsolute =
            sourceBar * kStepsPerBar + sourceStep;
        for (int offset = relation.minOffset;
             offset <= relation.maxOffset;
             ++offset) {
          const int targetAbsolute = sourceAbsolute + offset;
          if (!coordinateInPhrase(occupancy.barCount, targetAbsolute)) continue;
          const uint8_t targetBar = static_cast<uint8_t>(
              targetAbsolute / kStepsPerBar);
          const uint8_t targetStep = static_cast<uint8_t>(
              targetAbsolute % kStepsPerBar);
          if (!relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                         targetBar, targetStep) ||
              !isOnsetLegal(archetype, *targetLane, targetStep)) {
            continue;
          }
          if (occupancy.roleMasks[targetBar]
                  [static_cast<uint8_t>(relation.target)] &
              stepBit(targetStep)) {
            continue;
          }

          PhraseOccupancy trial = occupancy;
          if (!addStructuralCandidate(archetype, *targetLane,
                                      trial, targetBar, targetStep)) {
            continue;
          }
          const uint16_t deficit = respondDeficit(relation, trial);
          if (deficit >= currentDeficit) continue;

          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(targetBar,
                                        relation.target, targetStep));
          if (bestBar < 0 || deficit < bestDeficit ||
              (deficit == bestDeficit && rank > bestRank)) {
            bestBar = targetBar;
            bestStep = targetStep;
            bestDeficit = deficit;
            bestRank = rank;
            bestOccupancy = trial;
          }
        }
      }
    }

    if (bestBar < 0 || bestStep < 0) return false;
    occupancy = bestOccupancy;
  }
  return relationshipSatisfied(relation, occupancy);
}

bool repairHardRelationships(const RhythmArchetype& archetype,
                             PhraseOccupancy& occupancy,
                             uint32_t seed) {
  for (uint8_t pass = 0; pass < kMaxRelationships + 1; ++pass) {
    bool allSatisfied = true;
    for (uint8_t i = 0; i < archetype.relationshipCount; ++i) {
      const LaneRelationship& relation = archetype.relationships[i];
      if (relation.strength != ConstraintStrength::Hard ||
          relationshipSatisfied(relation, occupancy)) {
        continue;
      }
      allSatisfied = false;
      bool repaired = false;
      switch (relation.op) {
        case RelationshipOp::Exclude:
          repaired = repairExclude(archetype, relation, occupancy,
                                   seed ^ static_cast<uint32_t>(i));
          break;
        case RelationshipOp::Coincide:
          repaired = repairCoincide(archetype, relation, occupancy,
                                    seed ^ static_cast<uint32_t>(i));
          break;
        case RelationshipOp::Offset:
          repaired = repairOffset(archetype, relation, occupancy,
                                  seed ^ static_cast<uint32_t>(i));
          break;
        case RelationshipOp::Respond:
          repaired = repairRespond(archetype, relation, occupancy,
                                   seed ^ static_cast<uint32_t>(i));
          break;
        case RelationshipOp::FillGaps:
        default:
          return false;
      }
      if (!repaired) return false;
    }
    if (allSatisfied || hardRelationshipsSatisfied(archetype, occupancy)) {
      return true;
    }
  }
  return hardRelationshipsSatisfied(archetype, occupancy);
}

bool fillLaneMinimums(const RhythmArchetype& archetype,
                      PhraseOccupancy& occupancy,
                      uint32_t seed) {
  for (uint8_t pass = 0; pass < kRhythmRoleCount * 2; ++pass) {
    bool progress = false;
    bool complete = true;
    for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
      for (uint8_t laneIndex = 0;
           laneIndex < archetype.laneCount;
           ++laneIndex) {
        const LaneGrammar& lane = archetype.lanes[laneIndex];
        if (structuralCount(occupancy, bar, lane.role) >=
            lane.structuralMin) {
          continue;
        }
        complete = false;
        if (addBestStructuralCandidate(
                archetype, lane, occupancy, bar,
                seed ^ candidateCoordinate(bar, lane.role, pass))) {
          progress = true;
        }
      }
    }
    if (complete) return true;
    if (!progress) break;
  }

  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      if (structuralCount(occupancy, bar, lane.role) <
          lane.structuralMin) {
        return false;
      }
    }
  }
  return true;
}

void fillPreferredDensity(const RhythmArchetype& archetype,
                          PhraseOccupancy& occupancy,
                          uint8_t structuralTarget,
                          uint32_t seed) {
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    uint8_t guard = 0;
    while (totalStructural(occupancy, bar) < structuralTarget &&
           guard++ < kRhythmRoleCount * kStepsPerBar) {
      int bestLane = -1;
      int bestStep = -1;
      int32_t bestScore = -0x7FFFFFFF;
      for (uint8_t laneIndex = 0;
           laneIndex < archetype.laneCount;
           ++laneIndex) {
        const LaneGrammar& lane = archetype.lanes[laneIndex];
        if (structuralCount(occupancy, bar, lane.role) >=
            lane.structuralMax) {
          continue;
        }
        const StepMask current = occupancy.roleMasks[
            bar][static_cast<uint8_t>(lane.role)];
        const StepMask candidates = static_cast<StepMask>(
            structuralLegalMask(archetype, lane) & ~current);
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          if (!(candidates & stepBit(step)) ||
              !hardCandidateAdditionAllowed(archetype, occupancy,
                                            bar, lane.role, step)) {
            continue;
          }
          const int32_t score = candidateScore(
              archetype, lane, occupancy, bar, step,
              seed ^ static_cast<uint32_t>(guard));
          if (bestLane < 0 || score > bestScore) {
            bestLane = laneIndex;
            bestStep = step;
            bestScore = score;
          }
        }
      }
      if (bestLane < 0) break;
      if (!addStructuralCandidate(archetype,
                                  archetype.lanes[bestLane],
                                  occupancy, bar,
                                  static_cast<uint8_t>(bestStep))) {
        break;
      }
    }
  }
}

bool occupancyRespectsBaseBounds(const RhythmArchetype& archetype,
                                 const PhraseOccupancy& occupancy) {
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    uint16_t total = 0;
    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      const StepMask mask = occupancy.roleMasks[
          bar][static_cast<uint8_t>(lane.role)];
      const uint8_t count = bitCount16(mask);
      if ((lane.immutableAnchors & ~mask) ||
          (lane.canonicalAnchors & ~mask) ||
          (mask & ~structuralLegalMask(archetype, lane)) ||
          count < lane.structuralMin ||
          count > lane.structuralMax) {
        return false;
      }
      total += count;
    }
    if (total < archetype.density.structuralMin ||
        total > archetype.density.structuralMax) {
      return false;
    }
  }
  return hardRelationshipsSatisfied(archetype, occupancy);
}

bool establishIdentity(const RhythmArchetype& archetype,
                       const GenerationContext& context,
                       uint8_t phraseBars,
                       uint8_t structuralTarget,
                       PhraseRhythmIdentity& identity) {
  identity = {};
  identity.archetypeId = archetype.id;
  identity.phraseBars = phraseBars;
  identity.trajectoryId = kNoTrajectoryId;
  identity.protectedSpaceCount = archetype.protectedSpaceCount;
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    identity.protectedSpaces[i] = archetype.protectedSpaces[i];
  }

  PhraseOccupancy occupancy{};
  occupancy.barCount = phraseBars;
  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)] =
          anchorMask(lane);
      identity.canonicalCore[bar][static_cast<uint8_t>(lane.role)] =
          lane.canonicalAnchors;
    }
  }

  const uint32_t identitySeed = deriveGenerationSeed(
      context, archetype.id, GenerationDomain::RhythmIdentity);
  if (!fillLaneMinimums(archetype, occupancy, identitySeed)) return false;
  if (!repairHardRelationships(archetype, occupancy,
                               identitySeed ^ 0x52454C31u)) {
    return false;
  }
  fillPreferredDensity(archetype, occupancy, structuralTarget,
                       identitySeed ^ 0x44454E31u);
  if (!repairHardRelationships(archetype, occupancy,
                               identitySeed ^ 0x52454C32u)) {
    return false;
  }
  if (!occupancyRespectsBaseBounds(archetype, occupancy)) return false;

  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      identity.structuralCore[bar][role] =
          occupancy.roleMasks[bar][role];
    }
  }
  return true;
}

bool identityMatchesProtectedSpace(const RhythmArchetype& archetype,
                                   const PhraseRhythmIdentity& identity) {
  if (identity.protectedSpaceCount !=
      archetype.protectedSpaceCount) {
    return false;
  }
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    if (identity.protectedSpaces[i].steps !=
            archetype.protectedSpaces[i].steps ||
        identity.protectedSpaces[i].affectedRoles !=
            archetype.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  return true;
}

bool identityValidForArchetype(const RhythmArchetype& archetype,
                               const PhraseRhythmIdentity& identity) {
  if (identity.archetypeId != archetype.id ||
      identity.phraseBars == 0 ||
      identity.phraseBars > kMaxPhraseBars ||
      identity.trajectoryId != kNoTrajectoryId ||
      !(archetype.allowedPhraseBars &
        phraseBarsBit(identity.phraseBars)) ||
      !identityMatchesProtectedSpace(archetype, identity)) {
    return false;
  }

  PhraseOccupancy occupancy{};
  occupancy.barCount = identity.phraseBars;
  for (uint8_t bar = 0; bar < identity.phraseBars; ++bar) {
    for (uint8_t roleIndex = 0;
         roleIndex < kRhythmRoleCount;
         ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      const LaneGrammar* lane = laneFor(archetype, role);
      const StepMask structural =
          identity.structuralCore[bar][roleIndex];
      const StepMask canonical =
          identity.canonicalCore[bar][roleIndex];
      if (!lane) {
        if (structural || canonical) return false;
        continue;
      }
      if (canonical != lane->canonicalAnchors ||
          (lane->immutableAnchors & ~structural) ||
          (lane->canonicalAnchors & ~structural) ||
          (structural & ~structuralLegalMask(archetype, *lane))) {
        return false;
      }
      occupancy.roleMasks[bar][roleIndex] = structural;
    }
  }
  return occupancyRespectsBaseBounds(archetype, occupancy);
}

void copyStructuralFromIdentity(const PhraseRhythmIdentity& identity,
                                RhythmPhrasePlan& plan) {
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      plan.bars[bar].roles[role].structural =
          identity.structuralCore[bar][role];
    }
  }
}

void applyGatePolicies(const RhythmArchetype& archetype,
                       RhythmPhrasePlan& plan) {
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
      const StepMask onsets = static_cast<StepMask>(
          rolePlan.structural | rolePlan.secondary | rolePlan.ghosts);
      rolePlan.heldGate = static_cast<StepMask>(onsets & lane.heldGate);
      rolePlan.tieGate = static_cast<StepMask>(onsets & lane.tieGate);
      const StepMask explicitGateSites = static_cast<StepMask>(
          lane.shortGate | lane.heldGate | lane.tieGate);
      rolePlan.shortGate = static_cast<StepMask>(
          (onsets & lane.shortGate) |
          (rolePlan.ghosts & ~explicitGateSites));
    }
  }
}

bool addPlanSecondary(const RhythmArchetype& archetype,
                      RhythmPhrasePlan& plan,
                      PhraseOccupancy& occupancy,
                      uint8_t bar,
                      const LaneGrammar& lane,
                      uint8_t step) {
  RoleRhythmPlan& rolePlan =
      plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
  const StepMask bit = stepBit(step);
  if ((rolePlan.structural | rolePlan.secondary |
       rolePlan.ghosts) & bit) {
    return false;
  }
  if (!isOnsetLegal(archetype, lane, step) ||
      structuralCount(occupancy, bar, lane.role) >=
          lane.structuralMax ||
      totalStructural(occupancy, bar) >=
          archetype.density.structuralMax ||
      !hardCandidateAdditionAllowed(archetype, occupancy,
                                    bar, lane.role, step)) {
    return false;
  }

  PhraseOccupancy candidate = occupancy;
  candidate.roleMasks[bar][static_cast<uint8_t>(lane.role)] =
      static_cast<StepMask>(
          candidate.roleMasks[bar][static_cast<uint8_t>(lane.role)] | bit);
  if (!hardRelationshipsSatisfied(archetype, candidate)) return false;

  rolePlan.secondary = static_cast<StepMask>(
      rolePlan.secondary | bit);
  occupancy = candidate;
  return true;
}

bool addPlanGhost(const RhythmArchetype& archetype,
                  RhythmPhrasePlan& plan,
                  uint8_t bar,
                  const LaneGrammar& lane,
                  uint8_t step) {
  RoleRhythmPlan& rolePlan =
      plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
  const StepMask bit = stepBit(step);
  if ((rolePlan.structural | rolePlan.secondary |
       rolePlan.ghosts) & bit) {
    return false;
  }
  if (!isOnsetLegal(archetype, lane, step) ||
      bitCount16(rolePlan.ghosts) >= lane.ornamentMax ||
      totalOrnaments(plan, bar) >= archetype.density.ornamentMax) {
    return false;
  }
  rolePlan.ghosts = static_cast<StepMask>(rolePlan.ghosts | bit);
  return true;
}

uint8_t legacySecondaryBudget(const MutationBudget& budget) {
  return (budget.flags & AllowOptionalAdds) ? budget.maxAdds : 0;
}

uint8_t legacyGhostBudget(const MutationBudget& budget) {
  return (budget.flags & AllowGhostConversion) ? budget.maxAdds : 0;
}

uint8_t secondaryBudgetFor(const MutationBudget& budget) {
  return budget.maxSecondaryAdds != 0
             ? budget.maxSecondaryAdds
             : legacySecondaryBudget(budget);
}

uint8_t ghostBudgetFor(const RhythmArchetype& archetype,
                       RealizationLevel level) {
  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(level)];
  if (budget.maxGhostAdds != 0) return budget.maxGhostAdds;

  const uint8_t direct = legacyGhostBudget(budget);
  if (direct != 0 || level != RealizationLevel::P3Transformation) {
    return direct;
  }

  const MutationBudget& p2 = archetype.mutation.level[
      static_cast<uint8_t>(RealizationLevel::P2Variation)];
  return p2.maxGhostAdds != 0 ? p2.maxGhostAdds : legacyGhostBudget(p2);
}

void addVariationPass(const RhythmArchetype& archetype,
                      uint32_t seed,
                      RhythmPhrasePlan& plan,
                      PhraseOccupancy& occupancy,
                      uint8_t maxAdds,
                      bool secondary) {
  uint8_t additions = 0;
  for (uint8_t bar = 0;
       bar < plan.barCount && additions < maxAdds;
       ++bar) {
    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount && additions < maxAdds;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      const RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
      const StepMask available = static_cast<StepMask>(
          (lane.preferred | lane.optional) &
          ~(rolePlan.structural | rolePlan.secondary |
            rolePlan.ghosts) &
          ~lane.forbidden & ~protectedMask(archetype, lane.role));

      int bestStep = -1;
      int32_t bestScore = -0x7FFFFFFF;
      for (uint8_t step = 0; step < kStepsPerBar; ++step) {
        if (!(available & stepBit(step))) continue;
        const int32_t score = candidateScore(
            archetype, lane, occupancy, bar, step,
            seed ^ static_cast<uint32_t>(additions));
        if (bestStep < 0 || score > bestScore) {
          bestStep = step;
          bestScore = score;
        }
      }
      if (bestStep < 0) continue;

      const bool added = secondary
          ? addPlanSecondary(archetype, plan, occupancy, bar, lane,
                             static_cast<uint8_t>(bestStep))
          : addPlanGhost(archetype, plan, bar, lane,
                         static_cast<uint8_t>(bestStep));
      if (added) ++additions;
    }
  }
}

void addVariation(const RhythmArchetype& archetype,
                  uint32_t seed,
                  RhythmPhrasePlan& plan) {
  if (plan.level == RealizationLevel::P1Canonical) return;

  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(plan.level)];
  PhraseOccupancy occupancy = structuralOccupancy(plan);
  const uint8_t secondaryAdds = secondaryBudgetFor(budget);
  const uint8_t ghostAdds = ghostBudgetFor(archetype, plan.level);

  if (secondaryAdds != 0) {
    addVariationPass(archetype, seed,
                     plan, occupancy, secondaryAdds, true);
  }
  if (ghostAdds != 0) {
    const uint32_t ghostSeed = secondaryAdds != 0
        ? seed ^ 0x47484F31u
        : seed;
    addVariationPass(archetype, ghostSeed,
                     plan, occupancy, ghostAdds, false);
  }
}

bool requestValid(const RhythmRealizationRequest& request,
                  const RhythmArchetype*& archetype) {
  if (!request.catalog ||
      !validateRhythmCatalog(*request.catalog) ||
      request.archetypeId == kNoArchetypeId ||
      request.phraseBars == 0 ||
      request.phraseBars > kMaxPhraseBars ||
      !validLevel(request.level)) {
    return false;
  }
  archetype = archetypeFor(*request.catalog, request.archetypeId);
  if (!archetype ||
      !(archetype->allowedPhraseBars &
        phraseBarsBit(request.phraseBars))) {
    return false;
  }
  if (request.structuralDensityTarget != kNoStructuralDensityTarget &&
      (request.structuralDensityTarget < archetype->density.structuralMin ||
       request.structuralDensityTarget > archetype->density.structuralMax)) {
    return false;
  }
  if (request.reuseIdentity &&
      (request.reuseIdentity->phraseBars != request.phraseBars ||
       request.reuseIdentity->archetypeId != request.archetypeId)) {
    return false;
  }
  return true;
}

bool rolePlanIsEmpty(const RoleRhythmPlan& plan) {
  return plan.structural == 0 &&
         plan.secondary == 0 &&
         plan.ghosts == 0 &&
         plan.shortGate == 0 &&
         plan.heldGate == 0 &&
         plan.tieGate == 0 &&
         plan.accents == 0;
}

}  // namespace

uint8_t projectStructuralDensityTarget(const RhythmArchetype& archetype,
                                       uint8_t normalizedDensity) {
  if (normalizedDensity > 16u) return kNoStructuralDensityTarget;
  const uint8_t minimum = archetype.density.structuralMin;
  const uint8_t maximum = archetype.density.structuralMax;
  if (maximum <= minimum) return minimum;
  const uint16_t range = static_cast<uint16_t>(maximum - minimum);
  const uint16_t scaled = static_cast<uint16_t>(range * normalizedDensity);
  return static_cast<uint8_t>(minimum + (scaled + 8u) / 16u);
}

PhraseOccupancy structuralOccupancy(const RhythmPhrasePlan& plan) {
  PhraseOccupancy occupancy{};
  occupancy.barCount = plan.barCount;
  for (uint8_t bar = 0;
       bar < plan.barCount && bar < kMaxPhraseBars;
       ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      occupancy.roleMasks[bar][role] = static_cast<StepMask>(
          plan.bars[bar].roles[role].structural |
          plan.bars[bar].roles[role].secondary);
    }
  }
  return occupancy;
}

bool planRespectsProtectedSpace(const RhythmArchetype& archetype,
                                const RhythmPhrasePlan& plan) {
  if (plan.barCount == 0 || plan.barCount > kMaxPhraseBars) return false;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t roleIndex = 0;
         roleIndex < kRhythmRoleCount;
         ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      const RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[roleIndex];
      const StepMask allOnsets = static_cast<StepMask>(
          rolePlan.structural | rolePlan.secondary |
          rolePlan.ghosts);
      if (allOnsets & protectedMask(archetype, role)) return false;
    }
  }
  return true;
}

bool planRespectsLaneBounds(const RhythmArchetype& archetype,
                            const RhythmPhrasePlan& plan) {
  if (plan.barCount == 0 ||
      plan.barCount > kMaxPhraseBars ||
      !validLevel(plan.level) ||
      plan.trajectoryId != kNoTrajectoryId ||
      plan.intent != TransformationIntent::Auto) {
    return false;
  }

  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    if (plan.bars[bar].function != BarFunction::Statement) return false;

    for (uint8_t roleIndex = 0;
         roleIndex < kRhythmRoleCount;
         ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      if (!laneFor(archetype, role) &&
          !rolePlanIsEmpty(plan.bars[bar].roles[roleIndex])) {
        return false;
      }
    }

    uint16_t total = 0;
    uint16_t ornaments = 0;
    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      const RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
      const StepMask structural = static_cast<StepMask>(
          rolePlan.structural | rolePlan.secondary);
      if ((lane.immutableAnchors & ~structural) ||
          (lane.canonicalAnchors & ~structural) ||
          (structural & ~structuralLegalMask(archetype, lane))) {
        return false;
      }

      const uint8_t count = bitCount16(structural);
      if (count < lane.structuralMin ||
          count > lane.structuralMax ||
          bitCount16(rolePlan.ghosts) > lane.ornamentMax) {
        return false;
      }

      const StepMask allOnsets = static_cast<StepMask>(
          structural | rolePlan.ghosts);
      const StepMask explicitGateSites = static_cast<StepMask>(
          lane.shortGate | lane.heldGate | lane.tieGate);
      const StepMask expectedShort = static_cast<StepMask>(
          (allOnsets & lane.shortGate) |
          (rolePlan.ghosts & ~explicitGateSites));
      const StepMask expectedHeld = static_cast<StepMask>(
          allOnsets & lane.heldGate);
      const StepMask expectedTie = static_cast<StepMask>(
          allOnsets & lane.tieGate);
      if (rolePlan.shortGate != expectedShort ||
          rolePlan.heldGate != expectedHeld ||
          rolePlan.tieGate != expectedTie ||
          (rolePlan.shortGate & rolePlan.heldGate) ||
          (rolePlan.shortGate & rolePlan.tieGate) ||
          (rolePlan.heldGate & rolePlan.tieGate)) {
        return false;
      }

      total += count;
      ornaments += bitCount16(rolePlan.ghosts);
    }

    if (total < archetype.density.structuralMin ||
        total > archetype.density.structuralMax ||
        ornaments > archetype.density.ornamentMax) {
      return false;
    }
  }
  return true;
}

RhythmRealizationResult realizeRhythmPhrase(
    const RhythmRealizationRequest& request) {
  RhythmRealizationResult result{};
  const RhythmArchetype* archetype = nullptr;
  if (!requestValid(request, archetype)) return result;

  const uint8_t structuralTarget =
      request.structuralDensityTarget == kNoStructuralDensityTarget
          ? archetype->density.structuralPreferred
          : request.structuralDensityTarget;

  if (request.reuseIdentity) {
    if (!identityValidForArchetype(*archetype,
                                   *request.reuseIdentity)) {
      return result;
    }
    result.identity = *request.reuseIdentity;
  } else {
    if (!establishIdentity(*archetype,
                           request.generation,
                           request.phraseBars,
                           structuralTarget,
                           result.identity)) {
      return result;
    }
  }

  const uint32_t identitySeed = deriveGenerationSeed(
      request.generation, archetype->id,
      GenerationDomain::RhythmIdentity);
  const uint32_t variationSeed = deriveVariationSeed(
      identitySeed, request.level, request.phraseBars);

  result.plan.barCount = request.phraseBars;
  result.plan.trajectoryId = kNoTrajectoryId;
  result.plan.level = request.level;
  result.plan.intent = TransformationIntent::Auto;
  for (uint8_t bar = 0; bar < result.plan.barCount; ++bar) {
    result.plan.bars[bar].function = BarFunction::Statement;
  }

  copyStructuralFromIdentity(result.identity, result.plan);
  addVariation(*archetype,
               variationSeed ^ 0x56415232u,
               result.plan);
  applyGatePolicies(*archetype, result.plan);

  if (!planRespectsProtectedSpace(*archetype, result.plan) ||
      !planRespectsLaneBounds(*archetype, result.plan)) {
    return result;
  }
  const PhraseOccupancy occupancy =
      structuralOccupancy(result.plan);
  if (!hardRelationshipsSatisfied(*archetype, occupancy)) {
    return result;
  }

  bool sparse = false;
  for (uint8_t bar = 0; bar < result.plan.barCount; ++bar) {
    if (totalStructural(occupancy, bar) <
        archetype->density.structuralPreferred) {
      sparse = true;
      break;
    }
  }

  result.status = sparse
                      ? RealizationStatus::ValidButSparse
                      : RealizationStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
