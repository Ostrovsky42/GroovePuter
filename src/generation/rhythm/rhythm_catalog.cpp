#include "rhythm_catalog.h"

#include <cstddef>
#include <cstdint>

namespace GroovePuterRhythm {
namespace {

constexpr int kMaxPhraseOffset = kStepsPerBar * kMaxPhraseBars - 1;
constexpr uint16_t kMaxEventsPerBar = kStepsPerBar * kMaxLanes;

template <typename T>
bool validEnum(T v, T count) {
  return static_cast<uint8_t>(v) < static_cast<uint8_t>(count);
}

CatalogValidationResult fail(CatalogValidationError e,
                             uint16_t a = kNoArchetypeIndex,
                             uint8_t i = kNoItemIndex) {
  return {e, a, i};
}

uint8_t bits16(StepMask m) {
  uint8_t n = 0;
  while (m) {
    m = static_cast<StepMask>(m & (m - 1u));
    ++n;
  }
  return n;
}

uint8_t bits64(uint64_t m) {
  uint8_t n = 0;
  while (m) {
    m &= m - 1u;
    ++n;
  }
  return n;
}

const LaneGrammar* laneFor(const RhythmArchetype& a, RhythmRole role) {
  for (uint8_t i = 0; i < a.laneCount; ++i) {
    if (a.lanes[i].role == role) return &a.lanes[i];
  }
  return nullptr;
}

StepMask protectedFor(const RhythmArchetype& a, RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask bit = rhythmRoleBit(role);
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].affectedRoles & bit) {
      result = static_cast<StepMask>(result | a.protectedSpaces[i].steps);
    }
  }
  return result;
}

StepMask candidates(const RhythmArchetype& a, const LaneGrammar& l) {
  const StepMask positive = static_cast<StepMask>(
      l.immutableAnchors | l.canonicalAnchors | l.preferred | l.optional);
  return static_cast<StepMask>(
      positive & ~l.forbidden & ~protectedFor(a, l.role));
}

uint64_t expand(StepMask mask, uint8_t bars) {
  uint64_t out = 0;
  for (uint8_t bar = 0; bar < bars; ++bar) {
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (mask & stepBit(step)) {
        out |= uint64_t{1} << (bar * kStepsPerBar + step);
      }
    }
  }
  return out;
}

uint64_t phraseMask(uint8_t bars) {
  return bars == kMaxPhraseBars
             ? UINT64_MAX
             : ((uint64_t{1} << (bars * kStepsPerBar)) - 1u);
}

uint64_t reachable(uint64_t sources, int8_t lo, int8_t hi,
                   RelationshipScope scope, uint8_t bars) {
  uint64_t out = 0;
  const int total = bars * kStepsPerBar;
  for (int source = 0; source < total; ++source) {
    if (!(sources & (uint64_t{1} << source))) continue;
    for (int delta = lo; delta <= hi; ++delta) {
      const int target = source + delta;
      if (target < 0 || target >= total) continue;
      if (scope == RelationshipScope::BarLocal &&
          source / kStepsPerBar != target / kStepsPerBar) {
        continue;
      }
      out |= uint64_t{1} << target;
    }
  }
  return out;
}

uint8_t responsesInWindow(uint64_t targets, int source, int8_t lo, int8_t hi,
                          RelationshipScope scope, uint8_t bars) {
  uint8_t count = 0;
  const int total = bars * kStepsPerBar;
  for (int delta = lo; delta <= hi; ++delta) {
    const int target = source + delta;
    if (target < 0 || target >= total) continue;
    if (scope == RelationshipScope::BarLocal &&
        source / kStepsPerBar != target / kStepsPerBar) {
      continue;
    }
    if (targets & (uint64_t{1} << target)) ++count;
  }
  return count;
}

bool enoughPerBar(uint64_t mask, uint8_t bars, uint8_t minimum) {
  for (uint8_t bar = 0; bar < bars; ++bar) {
    if (bits64(mask & (uint64_t{0xFFFF} << (bar * kStepsPerBar))) <
        minimum) {
      return false;
    }
  }
  return true;
}

uint64_t responseWindowMask(int source, int8_t lo, int8_t hi,
                            RelationshipScope scope, uint8_t bars) {
  uint64_t mask = 0;
  const int total = bars * kStepsPerBar;
  for (int delta = lo; delta <= hi; ++delta) {
    const int target = source + delta;
    if (target < 0 || target >= total) continue;
    if (scope == RelationshipScope::BarLocal &&
        source / kStepsPerBar != target / kStepsPerBar) {
      continue;
    }
    mask |= uint64_t{1} << target;
  }
  return mask;
}

int responseOwner(int target, uint64_t sourceAnchors, uint64_t zone,
                  const LaneRelationship& r, uint8_t bars) {
  int owner = -1;
  int ownerDistance = kMaxPhraseOffset + 1;
  const int total = bars * kStepsPerBar;
  for (int source = 0; source < total; ++source) {
    const uint64_t sourceBit = uint64_t{1} << source;
    if (!(sourceAnchors & zone & sourceBit)) continue;
    if (!(responseWindowMask(source, r.minOffset, r.maxOffset,
                             r.scope, bars) &
          (uint64_t{1} << target))) {
      continue;
    }
    const int distance =
        target >= source ? target - source : source - target;
    if (distance < ownerDistance ||
        (distance == ownerDistance && (owner < 0 || source < owner))) {
      owner = source;
      ownerDistance = distance;
    }
  }
  return owner;
}

bool mandatoryRespondFeasible(uint64_t sourceAnchors,
                              uint64_t targetCandidates,
                              uint64_t targetAnchors,
                              uint64_t zone,
                              const LaneRelationship& r,
                              uint8_t bars) {
  uint8_t candidateOwned[kMaxPhraseBars * kStepsPerBar]{};
  uint8_t mandatoryOwned[kMaxPhraseBars * kStepsPerBar]{};
  const int total = bars * kStepsPerBar;

  for (int target = 0; target < total; ++target) {
    const uint64_t targetBit = uint64_t{1} << target;
    if (!(targetCandidates & targetBit)) continue;
    const int owner = responseOwner(target, sourceAnchors, zone, r, bars);
    if (owner < 0) continue;
    ++candidateOwned[owner];
    if (targetAnchors & targetBit) ++mandatoryOwned[owner];
  }

  for (int source = 0; source < total; ++source) {
    const uint64_t sourceBit = uint64_t{1} << source;
    if (!(sourceAnchors & zone & sourceBit)) continue;
    if (candidateOwned[source] < r.minResponsesPerWindow) return false;
    if (r.maxResponsesPerWindow != 0 &&
        mandatoryOwned[source] > r.maxResponsesPerWindow) {
      return false;
    }
  }
  return true;
}

bool hardFeasible(const RhythmArchetype& a,
                  const LaneRelationship& r,
                  uint8_t bars) {
  const LaneGrammar* src = laneFor(a, r.source);
  const LaneGrammar* dst = laneFor(a, r.target);
  if (!src || !dst) return false;

  uint64_t srcCandidates = expand(candidates(a, *src), bars);
  uint64_t dstCandidates = expand(candidates(a, *dst), bars);
  if (src->structuralMax == 0) srcCandidates = 0;
  if (dst->structuralMax == 0) dstCandidates = 0;

  const uint64_t srcAnchors = expand(static_cast<StepMask>(
      src->immutableAnchors | src->canonicalAnchors), bars);
  const uint64_t dstAnchors = expand(static_cast<StepMask>(
      dst->immutableAnchors | dst->canonicalAnchors), bars);
  const uint64_t zone = expand(r.zoneMask, bars);
  const uint64_t all = phraseMask(bars);

  if (r.op == RelationshipOp::Exclude) {
    if (srcAnchors & dstAnchors & zone) return false;

    const StepMask srcBarCandidates = candidates(a, *src);
    const StepMask dstBarCandidates = candidates(a, *dst);
    const StepMask srcBarAnchors = static_cast<StepMask>(
        src->immutableAnchors | src->canonicalAnchors);
    const StepMask dstBarAnchors = static_cast<StepMask>(
        dst->immutableAnchors | dst->canonicalAnchors);

    const StepMask srcAvailable = static_cast<StepMask>(
        srcBarCandidates & ~(dstBarAnchors & r.zoneMask));
    const StepMask dstAvailable = static_cast<StepMask>(
        dstBarCandidates & ~(srcBarAnchors & r.zoneMask));
    const uint8_t srcNeed = src->structuralMin > bits16(srcBarAnchors)
                                ? src->structuralMin
                                : bits16(srcBarAnchors);
    const uint8_t dstNeed = dst->structuralMin > bits16(dstBarAnchors)
                                ? dst->structuralMin
                                : bits16(dstBarAnchors);
    if (bits16(srcAvailable) < srcNeed || bits16(dstAvailable) < dstNeed) {
      return false;
    }

    const StepMask sharedConflict = static_cast<StepMask>(
        srcAvailable & dstAvailable & r.zoneMask);
    const uint8_t srcNonConflict = bits16(
        static_cast<StepMask>(srcAvailable & ~sharedConflict));
    const uint8_t dstNonConflict = bits16(
        static_cast<StepMask>(dstAvailable & ~sharedConflict));
    const uint8_t srcSharedNeed =
        srcNeed > srcNonConflict ? srcNeed - srcNonConflict : 0;
    const uint8_t dstSharedNeed =
        dstNeed > dstNonConflict ? dstNeed - dstNonConflict : 0;
    if (static_cast<uint16_t>(srcSharedNeed) + dstSharedNeed >
        bits16(sharedConflict)) {
      return false;
    }

    return enoughPerBar(dstCandidates & ~(srcAnchors & zone) & all,
                        bars, dst->structuralMin);
  }

  if (r.op == RelationshipOp::Offset) {
    const uint64_t reach = reachable(srcCandidates,
                                     r.minOffset,
                                     r.maxOffset,
                                     r.scope,
                                     bars);
    if (dstAnchors & zone & ~reach) return false;
    return enoughPerBar(dstCandidates & ((~zone & all) | reach),
                        bars, dst->structuralMin);
  }

  if (r.op == RelationshipOp::Respond) {
    if (!mandatoryRespondFeasible(srcAnchors,
                                  dstCandidates,
                                  dstAnchors,
                                  zone,
                                  r,
                                  bars)) {
      return false;
    }
    if (r.minResponsesPerWindow == 0) return true;

    uint64_t viableSource = 0;
    int mandatoryWindows = 0;
    const int total = bars * kStepsPerBar;
    for (int source = 0; source < total; ++source) {
      const uint64_t bit = uint64_t{1} << source;
      if (!(srcCandidates & bit)) continue;
      const bool constrained = (zone & bit) != 0;
      const uint8_t available = constrained
          ? responsesInWindow(dstCandidates,
                              source,
                              r.minOffset,
                              r.maxOffset,
                              r.scope,
                              bars)
          : r.minResponsesPerWindow;
      if (!constrained || available >= r.minResponsesPerWindow) {
        viableSource |= bit;
      }
      if (srcAnchors & zone & bit) {
        ++mandatoryWindows;
        if (available < r.minResponsesPerWindow) return false;
      }
    }
    if (!enoughPerBar(viableSource, bars, src->structuralMin)) return false;
    if (mandatoryWindows * r.minResponsesPerWindow >
        dst->structuralMax * bars) {
      return false;
    }
  }

  return true;
}

bool transformFunction(BarFunction f) {
  return f == BarFunction::Reduction ||
         f == BarFunction::Turnaround ||
         f == BarFunction::Break;
}

bool matchingIntent(BarFunction f, TransformationIntent i) {
  return (f == BarFunction::Reduction &&
          i == TransformationIntent::Reduce) ||
         (f == BarFunction::Turnaround &&
          i == TransformationIntent::Turnaround) ||
         (f == BarFunction::Break &&
          i == TransformationIntent::Break);
}

const BarTrajectory* trajectoryFor(const RhythmCatalogView& c,
                                   TrajectoryId id) {
  for (uint8_t i = 0; i < c.trajectoryCount; ++i) {
    if (c.trajectories[i].id == id) return &c.trajectories[i];
  }
  return nullptr;
}

CatalogValidationResult validateTrajectories(const RhythmCatalogView& c) {
  if (c.trajectoryCount && !c.trajectories) {
    return fail(CatalogValidationError::MissingTrajectoryArray);
  }

  for (uint8_t i = 0; i < c.trajectoryCount; ++i) {
    const BarTrajectory& t = c.trajectories[i];
    if (t.id == kNoTrajectoryId) {
      return fail(CatalogValidationError::InvalidTrajectoryId,
                  kNoArchetypeIndex,
                  i);
    }
    for (uint8_t previous = 0; previous < i; ++previous) {
      if (c.trajectories[previous].id == t.id) {
        return fail(CatalogValidationError::DuplicateTrajectoryId,
                    kNoArchetypeIndex,
                    i);
      }
    }
    if (!t.barCount || t.barCount > kMaxPhraseBars) {
      return fail(CatalogValidationError::InvalidTrajectoryBarCount,
                  kNoArchetypeIndex,
                  i);
    }
    for (uint8_t bar = 0; bar < t.barCount; ++bar) {
      if (!validEnum(t.bars[bar], BarFunction::Count)) {
        return fail(CatalogValidationError::InvalidTrajectoryBarFunction,
                    kNoArchetypeIndex,
                    i);
      }
    }
  }
  return {};
}

CatalogValidationResult validateLanes(const RhythmArchetype& a,
                                      uint16_t archetypeIndex) {
  if (!a.laneCount || a.laneCount > kMaxLanes) {
    return fail(CatalogValidationError::InvalidLaneCount,
                archetypeIndex);
  }
  if (!a.lanes) {
    return fail(CatalogValidationError::MissingLaneArray,
                archetypeIndex);
  }

  RhythmRoleMask seen = 0;
  for (uint8_t i = 0; i < a.laneCount; ++i) {
    const LaneGrammar& lane = a.lanes[i];
    if (!validEnum(lane.role, RhythmRole::Count)) {
      return fail(CatalogValidationError::InvalidLaneRole,
                  archetypeIndex,
                  i);
    }
    const RhythmRoleMask bit = rhythmRoleBit(lane.role);
    if (seen & bit) {
      return fail(CatalogValidationError::DuplicateLaneRole,
                  archetypeIndex,
                  i);
    }
    seen = static_cast<RhythmRoleMask>(seen | bit);
    if (!(a.activeRoles & bit)) {
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
    for (size_t x = 0; x < 5; ++x) {
      for (size_t y = x + 1; y < 5; ++y) {
        if (zones[x] & zones[y]) {
          return fail(CatalogValidationError::OverlappingLaneZones,
                      archetypeIndex,
                      i);
        }
      }
    }

    if (lane.structuralMin > lane.structuralMax ||
        lane.structuralMax > kStepsPerBar ||
        lane.ornamentMax > kStepsPerBar) {
      return fail(CatalogValidationError::InvalidLaneDensity,
                  archetypeIndex,
                  i);
    }
    if (bits16(static_cast<StepMask>(
            lane.immutableAnchors | lane.canonicalAnchors)) >
        lane.structuralMax) {
      return fail(CatalogValidationError::TooManyStructuralAnchors,
                  archetypeIndex,
                  i);
    }
  }

  return seen == a.activeRoles
             ? CatalogValidationResult{}
             : fail(CatalogValidationError::ActiveRoleMissingLane,
                    archetypeIndex);
}

CatalogValidationResult validateProtected(const RhythmArchetype& a,
                                          uint16_t archetypeIndex) {
  if (a.protectedSpaceCount > kMaxProtectedSpaces) {
    return fail(CatalogValidationError::TooManyProtectedSpaces,
                archetypeIndex);
  }
  if (a.protectedSpaceCount && !a.protectedSpaces) {
    return fail(CatalogValidationError::MissingProtectedSpaceArray,
                archetypeIndex);
  }

  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    const ProtectedSpace& space = a.protectedSpaces[i];
    if (!space.steps || !space.affectedRoles) {
      return fail(CatalogValidationError::EmptyProtectedSpace,
                  archetypeIndex,
                  i);
    }
    if ((space.affectedRoles & ~kAllRhythmRoles) ||
        (space.affectedRoles & ~a.activeRoles)) {
      return fail(CatalogValidationError::InvalidProtectedSpaceRoles,
                  archetypeIndex,
                  i);
    }

    for (uint8_t roleIndex = 0;
         roleIndex < kRhythmRoleCount;
         ++roleIndex) {
      const RhythmRole role = static_cast<RhythmRole>(roleIndex);
      if (!(space.affectedRoles & rhythmRoleBit(role))) continue;
      const LaneGrammar* lane = laneFor(a, role);
      if (!lane) {
        return fail(CatalogValidationError::ActiveRoleMissingLane,
                    archetypeIndex,
                    i);
      }
      if (space.steps & static_cast<StepMask>(
              lane->immutableAnchors | lane->canonicalAnchors)) {
        return fail(CatalogValidationError::ProtectedSpaceAnchorConflict,
                    archetypeIndex,
                    i);
      }
    }
  }
  return {};
}

CatalogValidationResult validateRelationship(const RhythmArchetype& a,
                                             uint16_t archetypeIndex,
                                             uint8_t relationshipIndex) {
  const LaneRelationship& r = a.relationships[relationshipIndex];
  if (!validEnum(r.source, RhythmRole::Count) ||
      !validEnum(r.target, RhythmRole::Count)) {
    return fail(CatalogValidationError::InvalidRelationshipRole,
                archetypeIndex,
                relationshipIndex);
  }
  if (!(a.activeRoles & rhythmRoleBit(r.source)) ||
      !(a.activeRoles & rhythmRoleBit(r.target))) {
    return fail(CatalogValidationError::RelationshipRoleNotActive,
                archetypeIndex,
                relationshipIndex);
  }
  if (r.source == r.target) {
    return fail(CatalogValidationError::SameRoleRelationship,
                archetypeIndex,
                relationshipIndex);
  }
  if (!validEnum(r.op, RelationshipOp::Count)) {
    return fail(CatalogValidationError::InvalidRelationshipOp,
                archetypeIndex,
                relationshipIndex);
  }
  if (!validEnum(r.strength, ConstraintStrength::Count)) {
    return fail(CatalogValidationError::InvalidConstraintStrength,
                archetypeIndex,
                relationshipIndex);
  }
  if (!validEnum(r.scope, RelationshipScope::Count)) {
    return fail(CatalogValidationError::InvalidRelationshipScope,
                archetypeIndex,
                relationshipIndex);
  }
  if (!r.zoneMask) {
    return fail(CatalogValidationError::EmptyRelationshipZone,
                archetypeIndex,
                relationshipIndex);
  }
  if ((r.strength == ConstraintStrength::Soft) != (r.weight != 0)) {
    return fail(CatalogValidationError::InvalidRelationshipWeight,
                archetypeIndex,
                relationshipIndex);
  }

  const bool offsetsOk =
      r.minOffset <= r.maxOffset &&
      r.minOffset >= -kMaxPhraseOffset &&
      r.maxOffset <= kMaxPhraseOffset &&
      (r.scope == RelationshipScope::Phrase ||
       (r.minOffset >= -(kStepsPerBar - 1) &&
        r.maxOffset <= (kStepsPerBar - 1)));
  const bool noCoincide = !r.minMatches && !r.maxMatches;
  const bool noRespond =
      !r.minResponsesPerWindow && !r.maxResponsesPerWindow;

  switch (r.op) {
    case RelationshipOp::Exclude:
      if (r.minOffset || r.maxOffset) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincide || !noRespond) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::Coincide: {
      if (r.minOffset || r.maxOffset) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if ((r.maxMatches && r.maxMatches < r.minMatches) ||
          !noRespond ||
          (r.strength == ConstraintStrength::Hard &&
           !r.minMatches && !r.maxMatches)) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }

      if (r.strength == ConstraintStrength::Hard) {
        const LaneGrammar* src = laneFor(a, r.source);
        const LaneGrammar* dst = laneFor(a, r.target);
        const StepMask possible = static_cast<StepMask>(
            candidates(a, *src) & candidates(a, *dst) & r.zoneMask);
        const StepMask mandatory = static_cast<StepMask>(
            (src->immutableAnchors | src->canonicalAnchors) &
            (dst->immutableAnchors | dst->canonicalAnchors) &
            r.zoneMask);

        for (uint8_t bars = 1; bars <= kMaxPhraseBars; ++bars) {
          if (!(a.allowedPhraseBars & phraseBarsBit(bars))) continue;
          const uint16_t possibleMatches =
              static_cast<uint16_t>(bits16(possible)) * bars;
          const uint16_t mandatoryMatches =
              static_cast<uint16_t>(bits16(mandatory)) * bars;
          const uint16_t sourceCapacity =
              static_cast<uint16_t>(src->structuralMax) * bars;
          const uint16_t targetCapacity =
              static_cast<uint16_t>(dst->structuralMax) * bars;

          if (possibleMatches < r.minMatches ||
              r.minMatches > sourceCapacity ||
              r.minMatches > targetCapacity ||
              (r.maxMatches && mandatoryMatches > r.maxMatches)) {
            return fail(CatalogValidationError::ImpossibleHardRelationship,
                        archetypeIndex,
                        relationshipIndex);
          }
        }
      }
      break;
    }

    case RelationshipOp::Offset:
      if (!offsetsOk) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincide || !noRespond) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::Respond:
      if (!offsetsOk) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincide ||
          (r.maxResponsesPerWindow &&
           r.maxResponsesPerWindow < r.minResponsesPerWindow)) {
        return fail(CatalogValidationError::InvalidRelationshipCardinality,
                    archetypeIndex,
                    relationshipIndex);
      }
      break;

    case RelationshipOp::FillGaps:
      if (r.strength == ConstraintStrength::Hard) {
        return fail(CatalogValidationError::HardFillGapsUnsupported,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (r.minOffset || r.maxOffset) {
        return fail(CatalogValidationError::InvalidRelationshipOffsets,
                    archetypeIndex,
                    relationshipIndex);
      }
      if (!noCoincide || !noRespond) {
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

  if (r.strength == ConstraintStrength::Hard &&
      r.op != RelationshipOp::Coincide &&
      r.op != RelationshipOp::FillGaps) {
    for (uint8_t bars = 1; bars <= kMaxPhraseBars; ++bars) {
      if ((a.allowedPhraseBars & phraseBarsBit(bars)) &&
          !hardFeasible(a, r, bars)) {
        return fail(CatalogValidationError::ImpossibleHardRelationship,
                    archetypeIndex,
                    relationshipIndex);
      }
    }
  }
  return {};
}

CatalogValidationResult validateRelationships(const RhythmArchetype& a,
                                              uint16_t archetypeIndex) {
  if (a.relationshipCount > kMaxRelationships) {
    return fail(CatalogValidationError::TooManyRelationships,
                archetypeIndex);
  }
  if (a.relationshipCount && !a.relationships) {
    return fail(CatalogValidationError::MissingRelationshipArray,
                archetypeIndex);
  }
  for (uint8_t i = 0; i < a.relationshipCount; ++i) {
    const CatalogValidationResult result =
        validateRelationship(a, archetypeIndex, i);
    if (!result) return result;
  }
  return {};
}

CatalogValidationResult validateTransforms(const RhythmArchetype& a,
                                           uint16_t archetypeIndex) {
  if (a.anchorTransformRuleCount > kMaxAnchorTransformRules) {
    return fail(CatalogValidationError::TooManyAnchorTransformRules,
                archetypeIndex);
  }
  if (a.anchorTransformRuleCount && !a.anchorTransformRules) {
    return fail(CatalogValidationError::MissingAnchorTransformArray,
                archetypeIndex);
  }

  for (uint8_t i = 0; i < a.anchorTransformRuleCount; ++i) {
    const AnchorTransformRule& rule = a.anchorTransformRules[i];
    if (!validEnum(rule.role, RhythmRole::Count)) {
      return fail(CatalogValidationError::InvalidAnchorTransformRole,
                  archetypeIndex,
                  i);
    }
    if (!(a.activeRoles & rhythmRoleBit(rule.role))) {
      return fail(CatalogValidationError::AnchorTransformRoleNotActive,
                  archetypeIndex,
                  i);
    }
    if (!validEnum(rule.barFunction, BarFunction::Count) ||
        !transformFunction(rule.barFunction)) {
      return fail(CatalogValidationError::InvalidAnchorTransformBarFunction,
                  archetypeIndex,
                  i);
    }
    if (!validEnum(rule.intent, TransformationIntent::Count) ||
        rule.intent == TransformationIntent::Auto ||
        !matchingIntent(rule.barFunction, rule.intent)) {
      return fail(CatalogValidationError::InvalidAnchorTransformIntent,
                  archetypeIndex,
                  i);
    }

    const StepMask mask = static_cast<StepMask>(
        rule.suppressibleCanonical | rule.displaceableCanonical);
    if (!mask) {
      return fail(CatalogValidationError::EmptyAnchorTransformRule,
                  archetypeIndex,
                  i);
    }
    const LaneGrammar* lane = laneFor(a, rule.role);
    if (!lane || (mask & ~lane->canonicalAnchors)) {
      return fail(CatalogValidationError::AnchorTransformOutsideCanonical,
                  archetypeIndex,
                  i);
    }
  }
  return {};
}

CatalogValidationResult validateTimingDensityMutation(
    const RhythmArchetype& a,
    uint16_t archetypeIndex) {
  if (!validEnum(a.timing.compatibility, TimingCompatibility::Count)) {
    return fail(CatalogValidationError::InvalidTimingCompatibility,
                archetypeIndex);
  }
  if ((a.timing.affectedRoles & ~kAllRhythmRoles) ||
      (a.timing.affectedRoles & ~a.activeRoles) ||
      (a.timing.sensitiveSteps && !a.timing.affectedRoles) ||
      (a.timing.compatibility != TimingCompatibility::StraightOnly &&
       (!a.timing.sensitiveSteps || !a.timing.affectedRoles))) {
    return fail(CatalogValidationError::InvalidTimingEligibilityRoles,
                archetypeIndex);
  }

  const DensityContract& density = a.density;
  if (density.structuralMin > density.structuralPreferred ||
      density.structuralPreferred > density.structuralMax ||
      density.structuralMax > kMaxEventsPerBar ||
      density.ornamentMax > kMaxEventsPerBar) {
    return fail(CatalogValidationError::InvalidDensityContract,
                archetypeIndex);
  }

  uint16_t laneMin = 0;
  uint16_t laneMax = 0;
  uint16_t anchors = 0;
  for (uint8_t i = 0; i < a.laneCount; ++i) {
    const LaneGrammar& lane = a.lanes[i];
    laneMin += lane.structuralMin;
    laneMax += lane.structuralMax;
    anchors += bits16(static_cast<StepMask>(
        lane.immutableAnchors | lane.canonicalAnchors));
    if (bits16(candidates(a, lane)) < lane.structuralMin) {
      return fail(CatalogValidationError::InvalidDensityContract,
                  archetypeIndex,
                  i);
    }
  }
  if (laneMin > density.structuralMax ||
      anchors > density.structuralMax ||
      density.structuralMin > laneMax) {
    return fail(CatalogValidationError::InvalidDensityContract,
                archetypeIndex);
  }

  for (uint8_t level = 0;
       level < static_cast<uint8_t>(RealizationLevel::Count);
       ++level) {
    const MutationBudget& budget = a.mutation.level[level];
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

    if (((budget.allowedIntents &
          transformationIntentBit(TransformationIntent::Reduce)) != 0) !=
            ((budget.flags & AllowReduction) != 0) ||
        ((budget.allowedIntents &
          transformationIntentBit(TransformationIntent::Turnaround)) != 0) !=
            ((budget.flags & AllowTurnaround) != 0) ||
        ((budget.allowedIntents &
          transformationIntentBit(TransformationIntent::Break)) != 0) !=
            ((budget.flags & AllowBreak) != 0)) {
      return fail(CatalogValidationError::InvalidMutationPolicy,
                  archetypeIndex,
                  level);
    }
  }
  return {};
}

CatalogValidationResult validateRefs(const RhythmCatalogView& c,
                                     const RhythmArchetype& a,
                                     uint16_t archetypeIndex) {
  if (!a.trajectoryCount || a.trajectoryCount > kMaxTrajectoryRefs) {
    return fail(a.trajectoryCount
                    ? CatalogValidationError::TooManyTrajectoryRefs
                    : CatalogValidationError::InvalidTrajectoryRef,
                archetypeIndex);
  }
  if (!a.trajectories) {
    return fail(CatalogValidationError::MissingTrajectoryRefArray,
                archetypeIndex);
  }

  RealizationLevelMask coverage[kMaxPhraseBars]{};
  for (uint8_t i = 0; i < a.trajectoryCount; ++i) {
    const TrajectoryRef& ref = a.trajectories[i];
    if (ref.id == kNoTrajectoryId) {
      return fail(CatalogValidationError::InvalidTrajectoryRef,
                  archetypeIndex,
                  i);
    }
    for (uint8_t previous = 0; previous < i; ++previous) {
      if (a.trajectories[previous].id == ref.id) {
        return fail(CatalogValidationError::DuplicateTrajectoryRef,
                    archetypeIndex,
                    i);
      }
    }

    const BarTrajectory* trajectory = trajectoryFor(c, ref.id);
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
    if (!(a.allowedPhraseBars & phraseBarsBit(trajectory->barCount))) {
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
      if (!(ref.allowedLevels & realizationLevelBit(
              static_cast<RealizationLevel>(level)))) {
        continue;
      }
      const MutationBudget& budget = a.mutation.level[level];
      if ((hasReduction &&
           (!(budget.flags & AllowReduction) ||
            !(budget.allowedIntents &
              transformationIntentBit(TransformationIntent::Reduce)))) ||
          (hasTurnaround &&
           (!(budget.flags & AllowTurnaround) ||
            !(budget.allowedIntents &
              transformationIntentBit(
                  TransformationIntent::Turnaround)))) ||
          (hasBreak &&
           (!(budget.flags & AllowBreak) ||
            !(budget.allowedIntents &
              transformationIntentBit(TransformationIntent::Break))))) {
        return fail(CatalogValidationError::TrajectoryLevelConflict,
                    archetypeIndex,
                    i);
      }
    }
  }

  for (uint8_t bars = 1; bars <= kMaxPhraseBars; ++bars) {
    if ((a.allowedPhraseBars & phraseBarsBit(bars)) &&
        coverage[bars - 1] != kAllRealizationLevels) {
      return fail(CatalogValidationError::MissingTrajectoryCoverage,
                  archetypeIndex);
    }
  }
  return {};
}

CatalogValidationResult validateArchetype(const RhythmCatalogView& c,
                                          uint16_t archetypeIndex) {
  const RhythmArchetype& a = c.archetypes[archetypeIndex];
  if (a.id == kNoArchetypeId) {
    return fail(CatalogValidationError::InvalidArchetypeId,
                archetypeIndex);
  }
  if (!validEnum(a.family, RhythmFamily::Count)) {
    return fail(CatalogValidationError::InvalidFamily,
                archetypeIndex);
  }
  if (!a.allowedPhraseBars ||
      (a.allowedPhraseBars & ~kAllPhraseBars)) {
    return fail(CatalogValidationError::InvalidPhraseBarsMask,
                archetypeIndex);
  }
  if (!a.activeRoles ||
      (a.activeRoles & ~kAllRhythmRoles)) {
    return fail(CatalogValidationError::InvalidActiveRoleMask,
                archetypeIndex);
  }

#define CHECK_RESULT(expression)                         \
  do {                                                   \
    const CatalogValidationResult result = (expression); \
    if (!result) return result;                          \
  } while (false)

  CHECK_RESULT(validateLanes(a, archetypeIndex));
  CHECK_RESULT(validateProtected(a, archetypeIndex));
  CHECK_RESULT(validateRelationships(a, archetypeIndex));
  CHECK_RESULT(validateTransforms(a, archetypeIndex));
  CHECK_RESULT(validateTimingDensityMutation(a, archetypeIndex));

#undef CHECK_RESULT

  return validateRefs(c, a, archetypeIndex);
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
