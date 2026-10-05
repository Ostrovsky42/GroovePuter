#ifndef GROOVEPUTER_GENERATION_ROLES_BAR_FUNCTION_ROLES_H
#define GROOVEPUTER_GENERATION_ROLES_BAR_FUNCTION_ROLES_H

#include <cstdint>

#include "../rhythm/rhythm_types.h"
#include "bass_rhythm.h"
#include "chord_rhythm.h"

// P0-B1: bass and chord follow the phrase bar function.
//
// The phrase owner already applies a bar function to the rhythm plan, but only the
// drum roles are materialized from it; bass and chord rhythm are realized separately
// and never saw the function. These pure functions apply a small, explicit intent to an
// ALREADY REALIZED bass or chord plan. They are deliberately minimal and experimental:
//
//   Statement, Repeat, Return, Response, RepeatWithGhosts  -> plan returned unchanged (bit-identical)
//   Break      bass: keep the EARLIEST existing attack in place (first-by-time is an
//                    experimental anchor, not a proven musical one), drop the rest;
//                    never creates a downbeat that was not there.  chord: no attacks.
//   Reduction  bass: drop at most one attack (the latest), keeping the anchor; a single
//                    attack stays.  chord: drop at most one attack; empty is allowed.
//   Build      bass: add at most one legal attack in the second half.  chord: add at most
//                    one hit, only for short (non-sustained) chord plans.
//   Turnaround bass: add at most one late (steps 13..15) attack.  chord: unchanged.
//
// Adding is an opportunity, never an obligation: without a legal place the plan is left
// alone. Removing an attack removes its continuation chain, so silence is real.
namespace GroovePuterRhythm {
namespace BarFunctionRoles {

inline uint8_t countSteps(StepMask mask) {
  uint8_t n = 0;
  for (uint8_t s = 0; s < kStepsPerBar; ++s) if (mask & stepBit(s)) ++n;
  return n;
}

inline int firstStep(StepMask mask) {
  for (uint8_t s = 0; s < kStepsPerBar; ++s) if (mask & stepBit(s)) return s;
  return -1;
}

inline int lastStep(StepMask mask) {
  for (int s = kStepsPerBar - 1; s >= 0; --s) if (mask & stepBit(static_cast<uint8_t>(s))) return s;
  return -1;
}

// Continuation steps that directly follow an attack at `step`.
inline StepMask continuationChain(StepMask continuations, uint8_t step) {
  StepMask chain = 0;
  for (uint8_t s = static_cast<uint8_t>(step + 1); s < kStepsPerBar && (continuations & stepBit(s)); ++s) {
    chain = static_cast<StepMask>(chain | stepBit(s));
  }
  return chain;
}

// Legal add step in [lo, hi]: free, not forbidden; among candidates the one farthest from
// every existing attack (ties: earliest). -1 when there is no legal place.
inline int bestAddStep(StepMask occupied, StepMask forbidden, StepMask attacks, uint8_t lo, uint8_t hi) {
  int best = -1;
  int bestDistance = -1;
  for (uint8_t s = lo; s <= hi && s < kStepsPerBar; ++s) {
    const StepMask bit = stepBit(s);
    if ((occupied | forbidden) & bit) continue;
    int distance = kStepsPerBar;
    for (uint8_t a = 0; a < kStepsPerBar; ++a) {
      if (!(attacks & stepBit(a))) continue;
      const int d = s > a ? s - a : a - s;
      if (d < distance) distance = d;
    }
    if (distance > bestDistance) { best = s; bestDistance = distance; }
  }
  return best;
}

inline BassRhythmPlan applyToBassPlan(BarFunction function, const BassRhythmPlan& in,
                                      StepMask kickOnsets, StepMask protectedSpace) {
  BassRhythmPlan plan = in;
  const StepMask occupied = static_cast<StepMask>(in.onsets | in.continuations);
  switch (function) {
    case BarFunction::Break: {
      const int keep = firstStep(in.onsets);
      if (keep < 0) break;
      plan.onsets = stepBit(static_cast<uint8_t>(keep));
      plan.continuations = static_cast<StepMask>(
          continuationChain(in.continuations, static_cast<uint8_t>(keep)) & in.continuations);
      break;
    }
    case BarFunction::Reduction: {
      if (countSteps(in.onsets) < 2) break;
      const int drop = lastStep(in.onsets);
      plan.onsets = static_cast<StepMask>(in.onsets & ~stepBit(static_cast<uint8_t>(drop)));
      plan.continuations = static_cast<StepMask>(
          in.continuations & ~continuationChain(in.continuations, static_cast<uint8_t>(drop)));
      break;
    }
    case BarFunction::Build:
    case BarFunction::Turnaround: {
      if (in.kickRelationship != RelationshipOp::Exclude || in.onsets == 0) break;
      const uint8_t lo = function == BarFunction::Build ? 8 : 13;
      const int add = bestAddStep(occupied, static_cast<StepMask>(protectedSpace | kickOnsets), in.onsets,
                                  lo, kStepsPerBar - 1);
      if (add >= 0) plan.onsets = static_cast<StepMask>(in.onsets | stepBit(static_cast<uint8_t>(add)));
      break;
    }
    default:
      break;
  }
  return plan;
}

// `baseBassOnsets` are the bass attacks BEFORE this function was applied: removing a bass
// attack must not free a position for the chord (that would read as fill, not as thinning).
inline ChordRhythmPlan applyToChordPlan(BarFunction function, const ChordRhythmPlan& in,
                                        StepMask baseBassOnsets, StepMask protectedSpace) {
  ChordRhythmPlan plan = in;
  switch (function) {
    case BarFunction::Break:
      plan.onsets = 0;
      plan.continuations = 0;
      plan.releasePoints = 0;
      break;
    case BarFunction::Reduction: {
      const int drop = lastStep(in.onsets);
      if (drop < 0) break;
      const StepMask chain = continuationChain(in.continuations, static_cast<uint8_t>(drop));
      StepMask span = static_cast<StepMask>(stepBit(static_cast<uint8_t>(drop)) | chain);
      plan.onsets = static_cast<StepMask>(in.onsets & ~stepBit(static_cast<uint8_t>(drop)));
      plan.continuations = static_cast<StepMask>(in.continuations & ~chain);
      // A release point that ended the removed hit goes with it (one step past the chain end).
      const int end = lastStep(span);
      if (end + 1 < kStepsPerBar) span = static_cast<StepMask>(span | stepBit(static_cast<uint8_t>(end + 1)));
      plan.releasePoints = static_cast<StepMask>(in.releasePoints & ~span);
      break;
    }
    case BarFunction::Build: {
      if (in.onsets == 0 || in.continuations != 0) break;  // only short, non-sustained chord plans
      const StepMask occupied = static_cast<StepMask>(in.onsets | in.continuations);
      const int add = bestAddStep(occupied, static_cast<StepMask>(protectedSpace | baseBassOnsets), in.onsets, 8,
                                  kStepsPerBar - 1);
      if (add >= 0) plan.onsets = static_cast<StepMask>(in.onsets | stepBit(static_cast<uint8_t>(add)));
      break;
    }
    default:
      break;
  }
  return plan;
}

}  // namespace BarFunctionRoles
}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_BAR_FUNCTION_ROLES_H
