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

bool validIntent(TransformationIntent intent) {
  return static_cast<uint8_t>(intent) <
         static_cast<uint8_t>(TransformationIntent::Count);
}

const RhythmArchetype* archetypeFor(const RhythmCatalogView& catalog,
                                    RhythmArchetypeId id) {
  for (uint16_t i = 0; i < catalog.archetypeCount; ++i) {
    if (catalog.archetypes[i].id == id) return &catalog.archetypes[i];
  }
  return nullptr;
}

const BarTrajectory* trajectoryFor(const RhythmCatalogView& catalog,
                                   TrajectoryId id) {
  for (uint8_t i = 0; i < catalog.trajectoryCount; ++i) {
    if (catalog.trajectories[i].id == id) return &catalog.trajectories[i];
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
  return static_cast<StepMask>(lane.immutableAnchors | lane.canonicalAnchors);
}

StepMask protectedMask(const RhythmArchetype& archetype, RhythmRole role) {
  StepMask mask = 0;
  const RhythmRoleMask roleMask = rhythmRoleBit(role);
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    if (archetype.protectedSpaces[i].affectedRoles & roleMask) {
      mask = static_cast<StepMask>(mask | archetype.protectedSpaces[i].steps);
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
  return bitCount16(occupancy.roleMasks[bar][static_cast<uint8_t>(role)]);
}

uint16_t totalStructural(const PhraseOccupancy& occupancy, uint8_t bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(occupancy.roleMasks[bar][role]);
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
      deterministicValue(seed, candidateCoordinate(bar, lane.role, step)) &
      0xFFu);
  return score;
}

bool addStructuralCandidate(const RhythmArchetype& archetype,
                            const LaneGrammar& lane,
                            PhraseOccupancy& occupancy,
                            uint8_t bar,
                            uint8_t step) {
  if (!isOnsetLegal(archetype, lane, step)) return false;
  const StepMask bit = stepBit(step);
  StepMask& mask = occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)];
  if (mask & bit) return true;
  if (bitCount16(mask) >= lane.structuralMax) return false;
  if (!hardCandidateAdditionAllowed(archetype, occupancy, bar, lane.role, step)) {
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
  StepMask& mask = occupancy.roleMasks[bar][static_cast<uint8_t>(role)];
  const StepMask bit = stepBit(step);
  if (!(mask & bit)) return true;
  if (bitCount16(mask) <= lane->structuralMin) return false;
  mask = static_cast<StepMask>(mask & ~bit);
  return true;
}

bool coordinateInPhrase(uint8_t barCount, int absolute) {
  return absolute >= 0 && absolute < static_cast<int>(barCount * kStepsPerBar);
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
          (deterministicValue(seed, candidateCoordinate(
               bar, relation.target, step)) & 1u) != 0;
      if (targetFirst) {
        if (dropIdentityEvent(archetype, relation.target, occupancy, bar, step)) {
          continue;
        }
        if (dropIdentityEvent(archetype, relation.source, occupancy, bar, step)) {
          continue;
        }
      } else {
        if (dropIdentityEvent(archetype, relation.source, occupancy, bar, step)) {
          continue;
        }
        if (dropIdentityEvent(archetype, relation.target, occupancy, bar, step)) {
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
        if (dropIdentityEvent(archetype, relation.target, occupancy, bar, step) ||
            dropIdentityEvent(archetype, relation.source, occupancy, bar, step)) {
          removed = true;
          break;
        }
      }
    }
    if (!removed) return false;
    matches = coincidenceCount(relation, occupancy);
  }

  uint8_t guard = 0;
  while (matches < relation.minMatches && guard++ < kMaxPhraseBars * kStepsPerBar) {
    bool added = false;
    for (uint8_t bar = 0; bar < occupancy.barCount && !added; ++bar) {
      const StepMask sourceMask = occupancy.roleMasks[
          bar][static_cast<uint8_t>(relation.source)];
      for (uint8_t step = 0; step < kStepsPerBar; ++step) {
        const StepMask bit = stepBit(step);
        if (!(relation.zoneMask & bit) || !(sourceMask & bit)) continue;
        if (occupancy.roleMasks[bar][static_cast<uint8_t>(relation.target)] & bit) {
          continue;
        }
        if (addStructuralCandidate(archetype, *targetLane, occupancy, bar, step)) {
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
          if (!(relation.zoneMask & bit) || !(targetMask & bit)) continue;
          if (occupancy.roleMasks[bar][static_cast<uint8_t>(relation.source)] & bit) {
            continue;
          }
          if (addStructuralCandidate(archetype, *sourceLane, occupancy, bar, step)) {
            added = true;
            break;
          }
        }
      }
    }
    if (!added) {
      // Last bounded option: create a new shared coordinate in legal space.
      int bestBar = -1;
      int bestStep = -1;
      uint32_t bestRank = 0;
      for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          const StepMask bit = stepBit(step);
          if (!(relation.zoneMask & bit) ||
              !isOnsetLegal(archetype, *sourceLane, step) ||
              !isOnsetLegal(archetype, *targetLane, step)) {
            continue;
          }
          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(bar, relation.target, step));
          if (bestBar < 0 || rank > bestRank) {
            bestBar = bar;
            bestStep = step;
            bestRank = rank;
          }
        }
      }
      if (bestBar >= 0 &&
          addStructuralCandidate(archetype, *sourceLane, occupancy,
                                 static_cast<uint8_t>(bestBar),
                                 static_cast<uint8_t>(bestStep)) &&
          addStructuralCandidate(archetype, *targetLane, occupancy,
                                 static_cast<uint8_t>(bestBar),
                                 static_cast<uint8_t>(bestStep))) {
        added = true;
      }
    }
    if (!added) return false;
    matches = coincidenceCount(relation, occupancy);
  }
  return matches >= relation.minMatches;
}

bool repairOffset(const RhythmArchetype& archetype,
                  const LaneRelationship& relation,
                  PhraseOccupancy& occupancy,
                  uint32_t seed) {
  const LaneGrammar* sourceLane = laneFor(archetype, relation.source);
  const LaneGrammar* targetLane = laneFor(archetype, relation.target);
  if (!sourceLane || !targetLane) return false;

  for (uint8_t targetBar = 0; targetBar < occupancy.barCount; ++targetBar) {
    StepMask targetMask = occupancy.roleMasks[
        targetBar][static_cast<uint8_t>(relation.target)];
    for (uint8_t targetStep = 0; targetStep < kStepsPerBar; ++targetStep) {
      const StepMask bit = stepBit(targetStep);
      if (!(targetMask & bit) || !(relation.zoneMask & bit) ||
          offsetTargetSupported(relation, occupancy, targetBar, targetStep)) {
        continue;
      }

      int bestAbsolute = -1;
      uint32_t bestRank = 0;
      const int targetAbsolute = targetBar * kStepsPerBar + targetStep;
      for (int offset = relation.minOffset; offset <= relation.maxOffset; ++offset) {
        const int sourceAbsolute = targetAbsolute - offset;
        if (!coordinateInPhrase(occupancy.barCount, sourceAbsolute)) continue;
        const uint8_t sourceBar = static_cast<uint8_t>(sourceAbsolute / kStepsPerBar);
        const uint8_t sourceStep = static_cast<uint8_t>(sourceAbsolute % kStepsPerBar);
        if (!relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                       targetBar, targetStep) ||
            !isOnsetLegal(archetype, *sourceLane, sourceStep)) {
          continue;
        }
        const uint32_t rank = deterministicValue(
            seed, candidateCoordinate(sourceBar, relation.source, sourceStep));
        if (bestAbsolute < 0 || rank > bestRank) {
          bestAbsolute = sourceAbsolute;
          bestRank = rank;
        }
      }

      bool repaired = false;
      if (bestAbsolute >= 0) {
        repaired = addStructuralCandidate(
            archetype, *sourceLane, occupancy,
            static_cast<uint8_t>(bestAbsolute / kStepsPerBar),
            static_cast<uint8_t>(bestAbsolute % kStepsPerBar));
      }
      if (!repaired) {
        repaired = dropIdentityEvent(archetype, relation.target, occupancy,
                                     targetBar, targetStep);
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
    int bestBar = -1;
    int bestStep = -1;
    uint32_t bestRank = 0;

    for (uint8_t sourceBar = 0; sourceBar < occupancy.barCount; ++sourceBar) {
      const StepMask sourceMask = occupancy.roleMasks[
          sourceBar][static_cast<uint8_t>(relation.source)];
      for (uint8_t sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
        if (!(sourceMask & stepBit(sourceStep)) ||
            !(relation.zoneMask & stepBit(sourceStep))) {
          continue;
        }
        const int sourceAbsolute = sourceBar * kStepsPerBar + sourceStep;
        for (int offset = relation.minOffset; offset <= relation.maxOffset; ++offset) {
          const int targetAbsolute = sourceAbsolute + offset;
          if (!coordinateInPhrase(occupancy.barCount, targetAbsolute)) continue;
          const uint8_t targetBar = static_cast<uint8_t>(targetAbsolute / kStepsPerBar);
          const uint8_t targetStep = static_cast<uint8_t>(targetAbsolute % kStepsPerBar);
          if (!relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                         targetBar, targetStep) ||
              !isOnsetLegal(archetype, *targetLane, targetStep)) {
            continue;
          }
          if (occupancy.roleMasks[targetBar][static_cast<uint8_t>(relation.target)] &
              stepBit(targetStep)) {
            continue;
          }
          if (!hardCandidateAdditionAllowed(archetype, occupancy, targetBar,
                                            relation.target, targetStep)) {
            continue;
          }
          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(targetBar, relation.target, targetStep));
          if (bestBar < 0 || rank > bestRank) {
            bestBar = targetBar;
            bestStep = targetStep;
            bestRank = rank;
          }
        }
      }
    }

    if (bestBar < 0 ||
        !addStructuralCandidate(archetype, *targetLane, occupancy,
                                static_cast<uint8_t>(bestBar),
                                static_cast<uint8_t>(bestStep))) {
      return false;
    }
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
      for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
        const LaneGrammar& lane = archetype.lanes[laneIndex];
        if (structuralCount(occupancy, bar, lane.role) >= lane.structuralMin) {
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
    for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      if (structuralCount(occupancy, bar, lane.role) < lane.structuralMin) {
        return false;
      }
    }
  }
  return true;
}

void fillPreferredDensity(const RhythmArchetype& archetype,
                          PhraseOccupancy& occupancy,
                          uint32_t seed) {
  for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
    uint8_t guard = 0;
    while (totalStructural(occupancy, bar) <
               archetype.density.structuralPreferred &&
           guard++ < kRhythmRoleCount * kStepsPerBar) {
      int bestLane = -1;
      int bestStep = -1;
      int32_t bestScore = -0x7FFFFFFF;
      for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
        const LaneGrammar& lane = archetype.lanes[laneIndex];
        if (structuralCount(occupancy, bar, lane.role) >= lane.structuralMax) {
          continue;
        }
        const StepMask current =
            occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)];
        const StepMask candidates = static_cast<StepMask>(
            structuralLegalMask(archetype, lane) & ~current);
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          if (!(candidates & stepBit(step)) ||
              !hardCandidateAdditionAllowed(archetype, occupancy, bar,
                                            lane.role, step)) {
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
      if (!addStructuralCandidate(archetype, archetype.lanes[bestLane],
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
    for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      const StepMask mask = occupancy.roleMasks[
          bar][static_cast<uint8_t>(lane.role)];
      const uint8_t count = bitCount16(mask);
      if ((lane.immutableAnchors & ~mask) ||
          (lane.canonicalAnchors & ~mask) ||
          (mask & ~structuralLegalMask(archetype, lane)) ||
          count < lane.structuralMin || count > lane.structuralMax) {
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
                       TrajectoryId pinnedTrajectoryId,
                       PhraseRhythmIdentity& identity) {
  identity = {};
  identity.archetypeId = archetype.id;
  identity.phraseBars = phraseBars;
  identity.trajectoryId = pinnedTrajectoryId;
  identity.protectedSpaceCount = archetype.protectedSpaceCount;
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    identity.protectedSpaces[i] = archetype.protectedSpaces[i];
  }

  PhraseOccupancy occupancy{};
  occupancy.barCount = phraseBars;
  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
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
  fillPreferredDensity(archetype, occupancy, identitySeed ^ 0x44454E31u);
  if (!repairHardRelationships(archetype, occupancy,
                               identitySeed ^ 0x52454C32u)) {
    return false;
  }
  if (!occupancyRespectsBaseBounds(archetype, occupancy)) return false;

  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      identity.structuralCore[bar][role] = occupancy.roleMasks[bar][role];
    }
  }
  return true;
}

bool identityMatchesProtectedSpace(const RhythmArchetype& archetype,
                                   const PhraseRhythmIdentity& identity) {
  if (identity.protectedSpaceCount != archetype.protectedSpaceCount) return false;
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    if (identity.protectedSpaces[i].steps != archetype.protectedSpaces[i].steps ||
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
      identity.phraseBars == 0 || identity.phraseBars > kMaxPhraseBars ||
      !(archetype.allowedPhraseBars & phraseBarsBit(identity.phraseBars)) ||
      !identityMatchesProtectedSpace(archetype, identity)) {
    return false;
  }

  PhraseOccupancy occupancy{};
  occupancy.barCount = identity.phraseBars;
  for (uint8_t bar = 0; bar < identity.phraseBars; ++bar) {
    for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      const LaneGrammar* lane = laneFor(archetype, role);
      const StepMask structural = identity.structuralCore[bar][roleIndex];
      const StepMask canonical = identity.canonicalCore[bar][roleIndex];
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

const TrajectoryRef* trajectoryRefFor(const RhythmArchetype& archetype,
                                      TrajectoryId id) {
  for (uint8_t i = 0; i < archetype.trajectoryCount; ++i) {
    if (archetype.trajectories[i].id == id) return &archetype.trajectories[i];
  }
  return nullptr;
}

const BarTrajectory* chooseTrajectory(const RhythmCatalogView& catalog,
                                      const RhythmArchetype& archetype,
                                      uint8_t phraseBars,
                                      RealizationLevel level,
                                      TrajectoryId pinned,
                                      const GenerationContext& context) {
  if (pinned != kNoTrajectoryId) {
    const TrajectoryRef* ref = trajectoryRefFor(archetype, pinned);
    const BarTrajectory* trajectory = trajectoryFor(catalog, pinned);
    if (!ref || !trajectory || trajectory->barCount != phraseBars ||
        !(ref->allowedLevels & realizationLevelBit(level))) {
      return nullptr;
    }
    return trajectory;
  }

  uint16_t totalWeight = 0;
  for (uint8_t i = 0; i < archetype.trajectoryCount; ++i) {
    const TrajectoryRef& ref = archetype.trajectories[i];
    const BarTrajectory* trajectory = trajectoryFor(catalog, ref.id);
    if (!trajectory || trajectory->barCount != phraseBars ||
        !(ref.allowedLevels & realizationLevelBit(level))) {
      continue;
    }
    totalWeight = static_cast<uint16_t>(totalWeight + ref.weight);
  }
  if (!totalWeight) return nullptr;

  const uint32_t evolutionSeed = deriveGenerationSeed(
      context, archetype.id, GenerationDomain::BarEvolution,
      static_cast<uint32_t>(level));
  uint16_t pick = static_cast<uint16_t>(
      deterministicValue(evolutionSeed, phraseBars) % totalWeight);
  for (uint8_t i = 0; i < archetype.trajectoryCount; ++i) {
    const TrajectoryRef& ref = archetype.trajectories[i];
    const BarTrajectory* trajectory = trajectoryFor(catalog, ref.id);
    if (!trajectory || trajectory->barCount != phraseBars ||
        !(ref.allowedLevels & realizationLevelBit(level))) {
      continue;
    }
    if (pick < ref.weight) return trajectory;
    pick = static_cast<uint16_t>(pick - ref.weight);
  }
  return nullptr;
}

TransformationIntent impliedIntent(BarFunction function) {
  switch (function) {
    case BarFunction::Response:
      return TransformationIntent::Response;
    case BarFunction::Reduction:
      return TransformationIntent::Reduce;
    case BarFunction::Build:
      return TransformationIntent::Build;
    case BarFunction::Turnaround:
      return TransformationIntent::Turnaround;
    case BarFunction::Break:
      return TransformationIntent::Break;
    default:
      return TransformationIntent::Auto;
  }
}

TransformationIntent effectiveIntent(const RhythmPhrasePlan& plan,
                                     uint8_t bar) {
  if (plan.intent != TransformationIntent::Auto) return plan.intent;
  return impliedIntent(plan.bars[bar].function);
}

bool intentAllowed(const RhythmArchetype& archetype,
                   RealizationLevel level,
                   TransformationIntent intent) {
  if (intent == TransformationIntent::Auto) return true;
  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(level)];
  return (budget.allowedIntents & transformationIntentBit(intent)) != 0;
}

TransformationIntent chooseAutoIntent(const RhythmArchetype& archetype,
                                      RealizationLevel level,
                                      const BarTrajectory& trajectory,
                                      uint32_t seed) {
  if (level == RealizationLevel::P1Canonical) {
    return TransformationIntent::Auto;
  }
  TransformationIntentMask candidates = 0;
  for (uint8_t bar = 0; bar < trajectory.barCount; ++bar) {
    const TransformationIntent intent = impliedIntent(trajectory.bars[bar]);
    if (intent != TransformationIntent::Auto) {
      candidates = static_cast<TransformationIntentMask>(
          candidates | transformationIntentBit(intent));
    }
  }
  candidates = static_cast<TransformationIntentMask>(
      candidates & archetype.mutation.level[static_cast<uint8_t>(level)].allowedIntents);
  if (!candidates) return TransformationIntent::Auto;

  uint8_t options[static_cast<uint8_t>(TransformationIntent::Count)]{};
  uint8_t count = 0;
  for (uint8_t value = 1;
       value < static_cast<uint8_t>(TransformationIntent::Count);
       ++value) {
    const TransformationIntent intent = static_cast<TransformationIntent>(value);
    if (candidates & transformationIntentBit(intent)) options[count++] = value;
  }
  if (!count) return TransformationIntent::Auto;
  return static_cast<TransformationIntent>(
      options[deterministicValue(seed, trajectory.id) % count]);
}

void copyStructuralFromIdentity(const PhraseRhythmIdentity& identity,
                                RhythmPhrasePlan& plan) {
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      plan.bars[bar].roles[role].structural = identity.structuralCore[bar][role];
    }
  }
}

void applyRepeatSemantics(const PhraseRhythmIdentity& identity,
                          RhythmPhrasePlan& plan) {
  uint8_t statementBar = 0;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    const BarFunction function = plan.bars[bar].function;
    if (function == BarFunction::Statement) {
      statementBar = bar;
      continue;
    }
    if (function == BarFunction::Repeat ||
        function == BarFunction::RepeatWithGhosts ||
        function == BarFunction::Return) {
      for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
        plan.bars[bar].roles[role].structural =
            identity.structuralCore[statementBar][role];
      }
    }
  }
}

bool transformRuleActive(const RhythmArchetype& archetype,
                         const RhythmPhrasePlan& plan,
                         uint8_t bar,
                         const AnchorTransformRule& rule) {
  if (plan.level == RealizationLevel::P1Canonical ||
      rule.barFunction != plan.bars[bar].function ||
      rule.intent != effectiveIntent(plan, bar) ||
      !intentAllowed(archetype, plan.level, rule.intent)) {
    return false;
  }
  return true;
}

bool canonicalMissingAllowed(const RhythmArchetype& archetype,
                             const RhythmPhrasePlan& plan,
                             uint8_t bar,
                             RhythmRole role,
                             StepMask missing) {
  if (!missing) return true;
  StepMask allowed = 0;
  for (uint8_t i = 0; i < archetype.anchorTransformRuleCount; ++i) {
    const AnchorTransformRule& rule = archetype.anchorTransformRules[i];
    if (rule.role != role || !transformRuleActive(archetype, plan, bar, rule)) {
      continue;
    }
    allowed = static_cast<StepMask>(
        allowed | rule.suppressibleCanonical | rule.displaceableCanonical);
  }
  return (missing & ~allowed) == 0;
}

uint8_t legallyMissingCanonicalCount(const RhythmArchetype& archetype,
                                     const RhythmPhrasePlan& plan,
                                     uint8_t bar) {
  uint8_t count = 0;
  for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
    const LaneGrammar& lane = archetype.lanes[laneIndex];
    const StepMask onset = static_cast<StepMask>(
        plan.bars[bar].roles[static_cast<uint8_t>(lane.role)].structural |
        plan.bars[bar].roles[static_cast<uint8_t>(lane.role)].secondary);
    const StepMask missing = static_cast<StepMask>(lane.canonicalAnchors & ~onset);
    if (canonicalMissingAllowed(archetype, plan, bar, lane.role, missing)) {
      count = static_cast<uint8_t>(count + bitCount16(missing));
    }
  }
  return count;
}

void applyCanonicalTransforms(const RhythmArchetype& archetype,
                              uint32_t seed,
                              RhythmPhrasePlan& plan) {
  MutationBudget budget =
      archetype.mutation.level[static_cast<uint8_t>(plan.level)];
  uint8_t drops = 0;
  for (uint8_t bar = 0; bar < plan.barCount && drops < budget.maxDrops; ++bar) {
    for (uint8_t ruleIndex = 0;
         ruleIndex < archetype.anchorTransformRuleCount && drops < budget.maxDrops;
         ++ruleIndex) {
      const AnchorTransformRule& rule = archetype.anchorTransformRules[ruleIndex];
      if (!transformRuleActive(archetype, plan, bar, rule)) continue;
      RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[static_cast<uint8_t>(rule.role)];
      StepMask candidates = static_cast<StepMask>(
          rolePlan.structural & rule.suppressibleCanonical);
      while (candidates && drops < budget.maxDrops) {
        int bestStep = -1;
        uint32_t bestRank = 0;
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          if (!(candidates & stepBit(step))) continue;
          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(bar, rule.role, step) ^ ruleIndex);
          if (bestStep < 0 || rank > bestRank) {
            bestStep = step;
            bestRank = rank;
          }
        }
        if (bestStep < 0) break;
        const StepMask bit = stepBit(static_cast<uint8_t>(bestStep));
        rolePlan.structural = static_cast<StepMask>(rolePlan.structural & ~bit);
        candidates = static_cast<StepMask>(candidates & ~bit);
        ++drops;
      }
    }
  }
}

bool addPlanSecondary(const RhythmArchetype& archetype,
                      RhythmPhrasePlan& plan,
                      PhraseOccupancy& occupancy,
                      uint8_t bar,
                      const LaneGrammar& lane,
                      uint8_t step) {
  const StepMask bit = stepBit(step);
  RoleRhythmPlan& rolePlan =
      plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
  if ((rolePlan.structural | rolePlan.secondary | rolePlan.ghosts) & bit) {
    return false;
  }
  if (!isOnsetLegal(archetype, lane, step) ||
      structuralCount(occupancy, bar, lane.role) >= lane.structuralMax ||
      !hardCandidateAdditionAllowed(archetype, occupancy, bar, lane.role, step)) {
    return false;
  }
  rolePlan.secondary = static_cast<StepMask>(rolePlan.secondary | bit);
  occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)] =
      static_cast<StepMask>(occupancy.roleMasks[bar][static_cast<uint8_t>(lane.role)] |
                            bit);
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
  if ((rolePlan.structural | rolePlan.secondary | rolePlan.ghosts) & bit ||
      !isOnsetLegal(archetype, lane, step) ||
      bitCount16(rolePlan.ghosts) >= lane.ornamentMax) {
    return false;
  }
  rolePlan.ghosts = static_cast<StepMask>(rolePlan.ghosts | bit);
  rolePlan.shortGate = static_cast<StepMask>(rolePlan.shortGate | bit);
  return true;
}

void addVariation(const RhythmArchetype& archetype,
                  uint32_t seed,
                  RhythmPhrasePlan& plan) {
  if (plan.level == RealizationLevel::P1Canonical) return;
  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(plan.level)];
  PhraseOccupancy occupancy = structuralOccupancy(plan);
  uint8_t additions = 0;

  for (uint8_t bar = 0; bar < plan.barCount && additions < budget.maxAdds; ++bar) {
    const BarFunction function = plan.bars[bar].function;
    if (function == BarFunction::Repeat) continue;

    for (uint8_t laneIndex = 0;
         laneIndex < archetype.laneCount && additions < budget.maxAdds;
         ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      const RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
      const StepMask available = static_cast<StepMask>(
          (lane.preferred | lane.optional) &
          ~(rolePlan.structural | rolePlan.secondary | rolePlan.ghosts) &
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

      const bool ghostOnly =
          function == BarFunction::RepeatWithGhosts ||
          ((budget.flags & AllowGhostConversion) &&
           !(budget.flags & AllowOptionalAdds));
      bool added = false;
      if (ghostOnly) {
        added = addPlanGhost(archetype, plan, bar, lane,
                             static_cast<uint8_t>(bestStep));
      } else if (budget.flags & AllowOptionalAdds) {
        added = addPlanSecondary(archetype, plan, occupancy, bar, lane,
                                 static_cast<uint8_t>(bestStep));
      } else if (budget.flags & AllowGhostConversion) {
        added = addPlanGhost(archetype, plan, bar, lane,
                             static_cast<uint8_t>(bestStep));
      }
      if (added) ++additions;
    }
  }
}

bool requestValid(const RhythmRealizationRequest& request,
                  const RhythmArchetype*& archetype) {
  if (!request.catalog || !validateRhythmCatalog(*request.catalog) ||
      request.archetypeId == kNoArchetypeId ||
      request.phraseBars == 0 || request.phraseBars > kMaxPhraseBars ||
      !validLevel(request.level) || !validIntent(request.intent)) {
    return false;
  }
  archetype = archetypeFor(*request.catalog, request.archetypeId);
  if (!archetype ||
      !(archetype->allowedPhraseBars & phraseBarsBit(request.phraseBars))) {
    return false;
  }
  if (request.level == RealizationLevel::P1Canonical &&
      request.intent != TransformationIntent::Auto) {
    return false;
  }
  if (request.intent != TransformationIntent::Auto &&
      !intentAllowed(*archetype, request.level, request.intent)) {
    return false;
  }
  if (request.reuseIdentity &&
      (request.reuseIdentity->phraseBars != request.phraseBars ||
       request.reuseIdentity->archetypeId != request.archetypeId ||
       (request.pinnedTrajectoryId != kNoTrajectoryId &&
        request.reuseIdentity->trajectoryId != request.pinnedTrajectoryId))) {
    return false;
  }
  return true;
}

}  // namespace

PhraseOccupancy structuralOccupancy(const RhythmPhrasePlan& plan) {
  PhraseOccupancy occupancy{};
  occupancy.barCount = plan.barCount;
  for (uint8_t bar = 0; bar < plan.barCount && bar < kMaxPhraseBars; ++bar) {
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
    for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      const RoleRhythmPlan& rolePlan = plan.bars[bar].roles[roleIndex];
      const StepMask allOnsets = static_cast<StepMask>(
          rolePlan.structural | rolePlan.secondary | rolePlan.ghosts);
      if (allOnsets & protectedMask(archetype, role)) return false;
    }
  }
  return true;
}

bool planRespectsLaneBounds(const RhythmArchetype& archetype,
                            const RhythmPhrasePlan& plan) {
  if (plan.barCount == 0 || plan.barCount > kMaxPhraseBars ||
      !validLevel(plan.level)) {
    return false;
  }
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    uint16_t total = 0;
    uint16_t ornaments = 0;
    for (uint8_t laneIndex = 0; laneIndex < archetype.laneCount; ++laneIndex) {
      const LaneGrammar& lane = archetype.lanes[laneIndex];
      const RoleRhythmPlan& rolePlan =
          plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
      const StepMask structural = static_cast<StepMask>(
          rolePlan.structural | rolePlan.secondary);
      if (lane.immutableAnchors & ~structural) return false;
      const StepMask missingCanonical = static_cast<StepMask>(
          lane.canonicalAnchors & ~structural);
      if (!canonicalMissingAllowed(archetype, plan, bar, lane.role,
                                   missingCanonical)) {
        return false;
      }
      if (structural & ~structuralLegalMask(archetype, lane)) return false;

      const uint8_t missingCount = bitCount16(missingCanonical);
      const uint8_t effectiveMin =
          lane.structuralMin > missingCount
              ? static_cast<uint8_t>(lane.structuralMin - missingCount)
              : 0;
      const uint8_t count = bitCount16(structural);
      if (count < effectiveMin || count > lane.structuralMax ||
          bitCount16(rolePlan.ghosts) > lane.ornamentMax) {
        return false;
      }
      total += count;
      ornaments += bitCount16(rolePlan.ghosts);
    }

    const uint8_t missing = legallyMissingCanonicalCount(archetype, plan, bar);
    const uint8_t effectiveGlobalMin =
        archetype.density.structuralMin > missing
            ? static_cast<uint8_t>(archetype.density.structuralMin - missing)
            : 0;
    if (total < effectiveGlobalMin || total > archetype.density.structuralMax ||
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

  if (request.reuseIdentity) {
    if (!identityValidForArchetype(*archetype, *request.reuseIdentity)) {
      return result;
    }
    result.identity = *request.reuseIdentity;
  } else {
    if (!establishIdentity(*archetype, request.generation,
                           request.phraseBars, request.pinnedTrajectoryId,
                           result.identity)) {
      return result;
    }
  }

  TrajectoryId pinned = result.identity.trajectoryId;
  if (!request.reuseIdentity && request.pinnedTrajectoryId != kNoTrajectoryId) {
    pinned = request.pinnedTrajectoryId;
  }
  const BarTrajectory* trajectory = chooseTrajectory(
      *request.catalog, *archetype, request.phraseBars, request.level,
      pinned, request.generation);
  if (!trajectory) return result;

  const uint32_t identitySeed = deriveGenerationSeed(
      request.generation, archetype->id, GenerationDomain::RhythmIdentity);
  const uint32_t variationSeed = deriveVariationSeed(
      identitySeed, request.level, request.phraseBars);

  result.plan.barCount = request.phraseBars;
  result.plan.trajectoryId = trajectory->id;
  result.plan.level = request.level;
  result.plan.intent = request.intent == TransformationIntent::Auto
                           ? chooseAutoIntent(*archetype, request.level,
                                              *trajectory, variationSeed)
                           : request.intent;
  for (uint8_t bar = 0; bar < request.phraseBars; ++bar) {
    result.plan.bars[bar].function = trajectory->bars[bar];
  }
  copyStructuralFromIdentity(result.identity, result.plan);
  applyRepeatSemantics(result.identity, result.plan);
  applyCanonicalTransforms(*archetype,
                           variationSeed ^ 0x54524E31u,
                           result.plan);
  addVariation(*archetype, variationSeed ^ 0x56415232u, result.plan);

  if (!planRespectsProtectedSpace(*archetype, result.plan) ||
      !planRespectsLaneBounds(*archetype, result.plan)) {
    return result;
  }
  const PhraseOccupancy occupancy = structuralOccupancy(result.plan);
  if (!hardRelationshipsSatisfied(*archetype, occupancy)) return result;

  bool sparse = false;
  for (uint8_t bar = 0; bar < result.plan.barCount; ++bar) {
    uint16_t total = totalStructural(occupancy, bar);
    const uint8_t missing = legallyMissingCanonicalCount(
        *archetype, result.plan, bar);
    const uint8_t effectivePreferred =
        archetype->density.structuralPreferred > missing
            ? static_cast<uint8_t>(
                  archetype->density.structuralPreferred - missing)
            : 0;
    if (total < effectivePreferred) sparse = true;
  }

  result.status = sparse ? RealizationStatus::ValidButSparse
                         : RealizationStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
