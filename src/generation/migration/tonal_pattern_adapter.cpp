#include "tonal_pattern_adapter.h"

namespace GroovePuterRhythm {
namespace {

bool validPlan(const TonalMaterializationPlan& plan,
               StepMask accentOnsets,
               StepMask slideIntoOnsets,
               const uint8_t* sourceOrder,
               uint8_t sourceOrderCount) {
  if ((plan.onsets & plan.continuations) != 0 ||
      (accentOnsets & static_cast<StepMask>(~plan.onsets)) != 0 ||
      (slideIntoOnsets & static_cast<StepMask>(~plan.onsets)) != 0 ||
      plan.onsetCount > kStepsPerBar ||
      sourceOrderCount > kStepsPerBar ||
      (sourceOrderCount != 0 && sourceOrder == nullptr)) {
    return false;
  }

  uint8_t ordinal = 0;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((plan.onsets & bit) != 0) {
      if (ordinal >= plan.onsetCount || plan.onsetSteps[ordinal] != step ||
          plan.midiNotes[ordinal] > 127) {
        return false;
      }
      ++ordinal;
      active = true;
      continue;
    }
    if ((plan.continuations & bit) != 0) {
      if (!active) return false;
      continue;
    }
    active = false;
  }
  return ordinal == plan.onsetCount;
}

}  // namespace

TonalPatternAdaptStatus adaptTonalPlanToSynthPattern(
    const SynthPattern& compatibilitySource,
    const TonalMaterializationPlan& tonalPlan,
    StepMask accentOnsets,
    StepMask slideIntoOnsets,
    SynthPattern& destination,
    const uint8_t* sourceOrder,
    uint8_t sourceOrderCount) {
  if (!validPlan(tonalPlan, accentOnsets, slideIntoOnsets,
                 sourceOrder, sourceOrderCount)) {
    return TonalPatternAdaptStatus::InvalidPlan;
  }

  // F-13: compatibilitySource and sourceOrder remain part of the adapter API so
  // existing planning/validation contracts stay intact, but incidental previous
  // destination contents must not become audible generation inputs. Motif/source
  // ordering is therefore planning metadata only in this tonal adapter until a
  // separate musical contract explicitly defines how it transforms pitch.
  (void)compatibilitySource;

  SynthPattern next{};
  SynthStep active{};
  bool hasActive = false;
  uint8_t ordinal = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const StepMask bit = stepBit(step);
    if ((tonalPlan.onsets & bit) != 0) {
      active = SynthStep{};
      active.note = static_cast<int8_t>(tonalPlan.midiNotes[ordinal]);
      if ((accentOnsets & bit) != 0) active.accent = true;
      if ((slideIntoOnsets & bit) != 0) active.slide = true;
      next.steps[step] = active;
      ++ordinal;
      hasActive = true;
      continue;
    }

    if ((tonalPlan.continuations & bit) != 0) {
      if (!hasActive) return TonalPatternAdaptStatus::InvalidPlan;
      SynthStep tied = active;
      tied.slide = true;
      tied.accent = false;
      tied.ghost = false;
      next.steps[step] = tied;
      continue;
    }
    hasActive = false;
  }

  destination = next;
  return TonalPatternAdaptStatus::Ok;
}

}  // namespace GroovePuterRhythm
