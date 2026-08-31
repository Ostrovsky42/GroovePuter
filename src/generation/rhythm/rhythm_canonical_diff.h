#ifndef GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_CANONICAL_DIFF_H
#define GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_CANONICAL_DIFF_H

#include <cstdint>
#include <type_traits>

#include "rhythm_realizer.h"

namespace GroovePuterRhythm {

// E2b is a canonical-relative consumer of the E2c delta vocabulary. It does
// not generate or choose candidates and it owns no lifecycle/history state.
enum class CanonicalRhythmDiffStatus : uint8_t {
  Ok = 0,
  InvalidContext,
  InvalidCanonicalMaterial,
  InvalidCandidateMaterial,
  UnrepresentableDelta,
  HiddenPhraseChange,
  OutputTooSmall,
  Count,
};

// Observational counts only. This is deliberately not another mutation budget:
// limits and permissions remain MutationPolicy / MutationBudget owned.
struct CanonicalRhythmDiffStats {
  uint16_t deltaCount = 0;
  uint8_t adds = 0;
  uint8_t drops = 0;
  uint8_t displacements = 0;
  uint8_t accentChanges = 0;
  uint8_t secondaryAdds = 0;
  uint8_t ghostAdds = 0;
};

struct CanonicalRhythmCandidateValidation {
  CanonicalRhythmDiffStatus diffStatus =
      CanonicalRhythmDiffStatus::InvalidContext;
  CanonicalRhythmDiffStats stats{};
  bool canonicalPlanValid = false;
  bool candidatePlanValid = false;
  bool budgetValid = false;
  bool legal = false;
};

// Deterministically compare one 16-step canonical bar against one candidate.
// Matching is bounded and same-lane. Exact same-kind onsets are paired first;
// remaining sources are considered in E2c canonical source order and the first
// legal target in logical step order wins. Only E2c-legal DISPLACE pairs are
// matched; everything else remains DROP + ADD/GHOST as applicable.
//
// deltas == nullptr with deltaCapacity == 0 is a supported stats-only mode.
// A non-null undersized output buffer returns OutputTooSmall after computing
// complete stats; no allocation or hidden cache is used.
CanonicalRhythmDiffStatus canonicalRhythmBarDiff(
    const RhythmArchetype& archetype,
    const RhythmBarPlan& canonical,
    const RhythmBarPlan& candidate,
    BarFunction function,
    TransformationIntent intent,
    RhythmMutationDelta* deltas,
    uint16_t deltaCapacity,
    CanonicalRhythmDiffStats& stats);

// Apply the existing MutationPolicy/MutationBudget to canonical-relative
// observational stats. P3 ghost compatibility preserves E1a's cumulative P2
// ornament fallback; no independent budget class is introduced.
bool canonicalRhythmBudgetValid(
    const MutationPolicy& policy,
    RealizationLevel level,
    TransformationIntent intent,
    const CanonicalRhythmDiffStats& stats);

// V0 / future consumer handoff. Only barIndex may differ between canonical and
// candidate phrase material. The function recomputes diff from canonical every
// call, validates the existing level policy, and delegates structural musical
// legality to rhythm_realizer's rhythmMutationPlanValid().
CanonicalRhythmCandidateValidation canonicalRhythmCandidateValid(
    const RhythmArchetype& archetype,
    const RhythmPhrasePlan& canonical,
    const RhythmPhrasePlan& candidate,
    uint8_t barIndex,
    RealizationLevel level,
    BarFunction function,
    TransformationIntent intent,
    RhythmMutationDelta* deltas,
    uint16_t deltaCapacity);

static_assert(std::is_trivially_copyable<CanonicalRhythmDiffStats>::value,
              "CanonicalRhythmDiffStats must remain bounded value data");
static_assert(std::is_trivially_copyable<CanonicalRhythmCandidateValidation>::value,
              "CanonicalRhythmCandidateValidation must remain bounded value data");
static_assert(sizeof(CanonicalRhythmDiffStats) <= 16,
              "E2b diff stats unexpectedly grew");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_CANONICAL_DIFF_H
