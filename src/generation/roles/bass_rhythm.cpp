#include "bass_rhythm.h"

#include <initializer_list>

namespace GroovePuterRhythm {
namespace {

constexpr StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps) result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

StepMask rotateAfter(StepMask source, uint8_t distance) {
  StepMask result = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((source & stepBit(step)) == 0) continue;
    const uint8_t target = static_cast<uint8_t>(step + distance);
    if (target < kStepsPerBar) {
      result = static_cast<StepMask>(result | stepBit(target));
    }
  }
  return result;
}

StepMask anchoredContinuations(StepMask onsets, StepMask continuations) {
  StepMask result = 0;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsets & bit) != 0) {
      active = true;
    } else if ((continuations & bit) != 0 && active) {
      result = static_cast<StepMask>(result | bit);
    } else {
      active = false;
    }
  }
  return result;
}

struct BassCandidates {
  BassRhythmId values[5]{};
  uint8_t count = 0;
};

BassCandidates candidatesFor(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor:
      return {{BassRhythmId::KickLock, BassRhythmId::OffbeatPush,
               BassRhythmId::RollingDrive, BassRhythmId::SustainAndDrop}, 4};
    case RhythmFamily::MachineSyncopation:
      return {{BassRhythmId::KickAnswer, BassRhythmId::GapFill,
               BassRhythmId::SyncopatedHook, BassRhythmId::RollingDrive}, 4};
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      return {{BassRhythmId::KickAnswer, BassRhythmId::GapFill,
               BassRhythmId::HalfTimePocket, BassRhythmId::SyncopatedHook}, 4};
    case RhythmFamily::HipHopBackbeat:
      return {{BassRhythmId::RootPulse, BassRhythmId::KickAnswer,
               BassRhythmId::SparseAnchor, BassRhythmId::HalfTimePocket,
               BassRhythmId::SustainAndDrop}, 5};
    case RhythmFamily::DubPulse:
      return {{BassRhythmId::RootPulse, BassRhythmId::GapFill,
               BassRhythmId::SparseAnchor, BassRhythmId::SustainAndDrop}, 4};
    case RhythmFamily::Funk16:
      return {{BassRhythmId::KickLock, BassRhythmId::KickAnswer,
               BassRhythmId::GapFill, BassRhythmId::SyncopatedHook}, 4};
    case RhythmFamily::SparsePulse:
      return {{BassRhythmId::RootPulse, BassRhythmId::SparseAnchor,
               BassRhythmId::SustainAndDrop}, 3};
    case RhythmFamily::Count:
      return {};
  }
  return {};
}

BassRhythmId selectId(const BassRhythmRequest& request) {
  if (request.requestedId != BassRhythmId::Auto) return request.requestedId;
  const BassCandidates candidates = candidatesFor(request.family);
  if (candidates.count == 0) return BassRhythmId::Auto;
  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::BassRhythmSelection,
      static_cast<uint32_t>(request.family));
  const uint8_t index = static_cast<uint8_t>(
      deterministicValue(seed, request.barOrdinal) % candidates.count);
  return candidates.values[index];
}

RelationshipOp relationshipFor(BassRhythmId id) {
  switch (id) {
    case BassRhythmId::KickLock: return RelationshipOp::Coincide;
    case BassRhythmId::KickAnswer: return RelationshipOp::Respond;
    case BassRhythmId::GapFill: return RelationshipOp::FillGaps;
    case BassRhythmId::OffbeatPush: return RelationshipOp::Offset;
    case BassRhythmId::RootPulse:
    case BassRhythmId::SparseAnchor:
    case BassRhythmId::RollingDrive:
    case BassRhythmId::HalfTimePocket:
    case BassRhythmId::SyncopatedHook:
    case BassRhythmId::SustainAndDrop:
      return RelationshipOp::Exclude;
    case BassRhythmId::Auto:
    case BassRhythmId::Count:
      break;
  }
  return RelationshipOp::Exclude;
}

}  // namespace

bool isValidBassRhythmId(BassRhythmId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(BassRhythmId::Count)) return false;
  return allowAuto || id != BassRhythmId::Auto;
}

BassRhythmResult realizeBassRhythm(const BassRhythmRequest& request) {
  BassRhythmResult result{};
  if (request.archetypeId == kNoArchetypeId ||
      static_cast<uint8_t>(request.family) >=
          static_cast<uint8_t>(RhythmFamily::Count) ||
      !isValidBassRhythmId(request.requestedId)) {
    return result;
  }

  const BassRhythmId id = selectId(request);
  if (!isValidBassRhythmId(id, false)) return result;

  StepMask onsets = 0;
  StepMask continuations = 0;
  switch (id) {
    case BassRhythmId::RootPulse:
      onsets = mask({0, 8});
      break;
    case BassRhythmId::KickLock:
      onsets = request.kickOnsets;
      break;
    case BassRhythmId::KickAnswer:
      onsets = rotateAfter(request.kickOnsets, 2);
      onsets = static_cast<StepMask>(onsets & ~request.kickOnsets);
      break;
    case BassRhythmId::GapFill:
      onsets = static_cast<StepMask>(mask({2, 6, 10, 14}) &
                                     ~request.kickOnsets);
      break;
    case BassRhythmId::OffbeatPush:
      onsets = mask({2, 6, 10, 14});
      break;
    case BassRhythmId::SparseAnchor:
      onsets = (request.allowEmptyBar && (request.barOrdinal & 1u) != 0u)
                   ? 0
                   : stepBit((request.barOrdinal & 2u) != 0u ? 8 : 0);
      break;
    case BassRhythmId::RollingDrive:
      onsets = mask({0, 2, 4, 6, 8, 10, 12, 14});
      break;
    case BassRhythmId::HalfTimePocket:
      onsets = mask({0, 7, 10});
      break;
    case BassRhythmId::SyncopatedHook:
      onsets = mask({0, 3, 7, 10, 14});
      break;
    case BassRhythmId::SustainAndDrop:
      if (!(request.allowEmptyBar && (request.barOrdinal % 4u) == 1u)) {
        onsets = stepBit(0);
        continuations = mask({1, 2, 3, 4, 5, 6, 7});
        if ((request.barOrdinal & 1u) == 0u) {
          onsets = static_cast<StepMask>(onsets | stepBit(12));
          continuations = static_cast<StepMask>(
              continuations | mask({13, 14, 15}));
        }
      }
      break;
    case BassRhythmId::Auto:
    case BassRhythmId::Count:
      return result;
  }

  onsets = static_cast<StepMask>(onsets & ~request.protectedSpace);
  continuations = static_cast<StepMask>(
      continuations & ~request.protectedSpace & ~onsets);
  continuations = anchoredContinuations(onsets, continuations);
  result.plan.id = id;
  result.plan.kickRelationship = relationshipFor(id);
  result.plan.onsets = onsets;
  result.plan.continuations = continuations;
  result.status = onsets == 0 ? BassRhythmStatus::ValidButEmpty
                              : BassRhythmStatus::Ok;
  return result;
}

const char* bassRhythmName(BassRhythmId id) {
  switch (id) {
    case BassRhythmId::Auto: return "AUTO";
    case BassRhythmId::RootPulse: return "ROOT PULSE";
    case BassRhythmId::KickLock: return "KICK LOCK";
    case BassRhythmId::KickAnswer: return "KICK ANSWER";
    case BassRhythmId::GapFill: return "GAP FILL";
    case BassRhythmId::OffbeatPush: return "OFFBEAT PUSH";
    case BassRhythmId::SparseAnchor: return "SPARSE ANCHOR";
    case BassRhythmId::RollingDrive: return "ROLLING DRIVE";
    case BassRhythmId::HalfTimePocket: return "HALF-TIME POCKET";
    case BassRhythmId::SyncopatedHook: return "SYNCOPATED HOOK";
    case BassRhythmId::SustainAndDrop: return "SUSTAIN/DROP";
    case BassRhythmId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
