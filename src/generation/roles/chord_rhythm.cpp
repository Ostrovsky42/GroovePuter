#include "chord_rhythm.h"

#include <initializer_list>

namespace GroovePuterRhythm {
namespace {

constexpr StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps) result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

StepMask respondAfter(StepMask source, uint8_t distance) {
  StepMask result = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((source & stepBit(step)) == 0) continue;
    const uint8_t target = static_cast<uint8_t>(step + distance);
    if (target < kStepsPerBar) result = static_cast<StepMask>(result | stepBit(target));
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

struct Candidates {
  ChordRhythmId values[5]{};
  uint8_t count = 0;
};

Candidates candidatesFor(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor:
      return {{ChordRhythmId::HalfBarChange, ChordRhythmId::OffbeatStab,
               ChordRhythmId::BackbeatStab, ChordRhythmId::SyncopatedComp}, 4};
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Funk16:
      return {{ChordRhythmId::OffbeatStab, ChordRhythmId::AnticipatedChange,
               ChordRhythmId::SparseChordReply,
               ChordRhythmId::SyncopatedComp}, 4};
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      return {{ChordRhythmId::HeldPad, ChordRhythmId::BackbeatStab,
               ChordRhythmId::SparseChordReply,
               ChordRhythmId::AnticipatedChange}, 4};
    case RhythmFamily::HipHopBackbeat:
      return {{ChordRhythmId::HeldPad, ChordRhythmId::WholeBarHold,
               ChordRhythmId::SparseChordReply,
               ChordRhythmId::BackbeatStab}, 4};
    case RhythmFamily::DubPulse:
      return {{ChordRhythmId::WholeBarHold, ChordRhythmId::OffbeatStab,
               ChordRhythmId::SparseChordReply,
               ChordRhythmId::DubChordSpace}, 4};
    case RhythmFamily::SparsePulse:
      return {{ChordRhythmId::HeldPad, ChordRhythmId::WholeBarHold,
               ChordRhythmId::SparseChordReply}, 3};
    case RhythmFamily::Count:
      return {};
  }
  return {};
}

ChordRhythmId selectId(const ChordRhythmRequest& request) {
  if (request.requestedId != ChordRhythmId::Auto) return request.requestedId;
  const Candidates candidates = candidatesFor(request.family);
  if (candidates.count == 0) return ChordRhythmId::Auto;
  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::ChordRhythmSelection,
      static_cast<uint32_t>(request.family));
  return candidates.values[deterministicValue(seed, request.barOrdinal) %
                           candidates.count];
}

}  // namespace

bool isValidChordRhythmId(ChordRhythmId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(ChordRhythmId::Count)) return false;
  return allowAuto || id != ChordRhythmId::Auto;
}

ChordRhythmResult realizeChordRhythm(const ChordRhythmRequest& request) {
  ChordRhythmResult result{};
  if (request.archetypeId == kNoArchetypeId ||
      static_cast<uint8_t>(request.family) >=
          static_cast<uint8_t>(RhythmFamily::Count) ||
      !isValidChordRhythmId(request.requestedId)) {
    return result;
  }
  const ChordRhythmId id = selectId(request);
  if (!isValidChordRhythmId(id, false)) return result;

  StepMask onsets = 0;
  StepMask continuations = 0;
  StepMask releases = 0;
  switch (id) {
    case ChordRhythmId::HeldPad:
      onsets = stepBit(0);
      continuations = mask({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
      releases = stepBit(12);
      break;
    case ChordRhythmId::WholeBarHold:
      onsets = stepBit(0);
      continuations = mask({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
      break;
    case ChordRhythmId::HalfBarChange:
      onsets = mask({0, 8});
      continuations = mask({1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15});
      break;
    case ChordRhythmId::OffbeatStab:
      onsets = mask({2, 6, 10, 14});
      break;
    case ChordRhythmId::BackbeatStab:
      onsets = mask({4, 12});
      break;
    case ChordRhythmId::AnticipatedChange:
      onsets = mask({7, 15});
      break;
    case ChordRhythmId::SparseChordReply:
      onsets = respondAfter(request.bassOnsets, 2);
      onsets = static_cast<StepMask>(onsets & ~request.bassOnsets);
      break;
    case ChordRhythmId::DubChordSpace:
      onsets = (request.allowEmptyBar && (request.barOrdinal & 1u) != 0u)
                   ? 0
                   : mask({6, 14});
      break;
    case ChordRhythmId::SyncopatedComp:
      onsets = mask({1, 4, 7, 10, 13});
      break;
    case ChordRhythmId::Auto:
    case ChordRhythmId::Count:
      return result;
  }

  const StepMask blocked = static_cast<StepMask>(
      request.protectedSpace | request.bassOnsets);
  onsets = static_cast<StepMask>(onsets & ~blocked);
  continuations = static_cast<StepMask>(
      continuations & ~request.protectedSpace & ~onsets);
  continuations = anchoredContinuations(onsets, continuations);
  releases = onsets == 0
      ? StepMask{0}
      : static_cast<StepMask>(releases & ~onsets & ~continuations);

  result.plan.id = id;
  result.plan.onsets = onsets;
  result.plan.continuations = continuations;
  result.plan.releasePoints = releases;
  result.status = onsets == 0 ? ChordRhythmStatus::ValidButEmpty
                              : ChordRhythmStatus::Ok;
  return result;
}

const char* chordRhythmName(ChordRhythmId id) {
  switch (id) {
    case ChordRhythmId::Auto: return "AUTO";
    case ChordRhythmId::HeldPad: return "HELD PAD";
    case ChordRhythmId::WholeBarHold: return "WHOLE BAR HOLD";
    case ChordRhythmId::HalfBarChange: return "HALF BAR CHANGE";
    case ChordRhythmId::OffbeatStab: return "OFFBEAT STAB";
    case ChordRhythmId::BackbeatStab: return "BACKBEAT STAB";
    case ChordRhythmId::AnticipatedChange: return "ANTICIPATED CHANGE";
    case ChordRhythmId::SparseChordReply: return "SPARSE CHORD REPLY";
    case ChordRhythmId::DubChordSpace: return "DUB CHORD SPACE";
    case ChordRhythmId::SyncopatedComp: return "SYNCOPATED COMP";
    case ChordRhythmId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
