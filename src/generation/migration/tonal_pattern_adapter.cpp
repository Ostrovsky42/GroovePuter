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

uint8_t collectCompatibilityEvents(const SynthPattern& source,
                                   SynthStep* events) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (source.steps[step].note < 0) continue;
    events[count++] = source.steps[step];
  }
  return count;
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

  SynthStep sourceEvents[SynthPattern::kSteps]{};
  const uint8_t sourceCount =
      collectCompatibilityEvents(compatibilitySource, sourceEvents);

  SynthPattern next{};
  SynthStep active{};
  bool hasActive = false;
  uint8_t ordinal = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const StepMask bit = stepBit(step);
    if ((tonalPlan.onsets & bit) != 0) {
      active = SynthStep{};
      if (sourceCount != 0) {
        uint8_t selected = ordinal;
        if (sourceOrderCount != 0)
          selected = sourceOrder[ordinal % sourceOrderCount];
        active = sourceEvents[selected % sourceCount];
      }
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
