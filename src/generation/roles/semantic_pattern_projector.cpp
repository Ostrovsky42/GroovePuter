#include "semantic_pattern_projector.h"

namespace GroovePuterRhythm {

SemanticPatternProjectStatus projectLegacyPitchPattern(
    const SynthPattern& source,
    StepMask onsetMask,
    StepMask continuationMask,
    SynthPattern& destination) {
  return projectLegacyPitchPatternWithOrder(
      source, onsetMask, continuationMask, nullptr, 0, destination);
}

SemanticPatternProjectStatus projectLegacyPitchPatternWithOrder(
    const SynthPattern& source,
    StepMask onsetMask,
    StepMask continuationMask,
    const uint8_t* sourceOrder,
    uint8_t sourceOrderCount,
    SynthPattern& destination) {
  if ((onsetMask & continuationMask) != 0 ||
      sourceOrderCount > SynthPattern::kSteps ||
      (sourceOrderCount != 0 && sourceOrder == nullptr)) {
    return SemanticPatternProjectStatus::InvalidPlan;
  }

  SynthStep sourceEvents[SynthPattern::kSteps]{};
  uint8_t sourceCount = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (source.steps[step].note >= 0) {
      sourceEvents[sourceCount++] = source.steps[step];
    }
  }
  if (onsetMask != 0 && sourceCount == 0) {
    return SemanticPatternProjectStatus::MissingPitchSource;
  }

  SynthPattern next{};
  uint8_t sourceIndex = 0;
  SynthStep active{};
  bool hasActive = false;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsetMask & bit) != 0) {
      uint8_t selected = sourceIndex;
      if (sourceOrderCount != 0) {
        selected = sourceOrder[sourceIndex % sourceOrderCount];
      }
      active = sourceEvents[selected % sourceCount];
      ++sourceIndex;
      next.steps[step] = active;
      hasActive = true;
      continue;
    }
    if ((continuationMask & bit) != 0) {
      if (!hasActive) return SemanticPatternProjectStatus::InvalidPlan;
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
  return SemanticPatternProjectStatus::Ok;
}

FeelInterpretStatus applyFeelToSemanticPattern(
    RhythmRole role,
    StepMask onsetMask,
    FeelProfileId profile,
    uint8_t amount,
    const GenerationContext& generation,
    SynthPattern& destination) {
  if (static_cast<uint8_t>(role) >= kRhythmRoleCount || amount > 100) {
    return FeelInterpretStatus::InvalidPhrase;
  }
  FeelPhrase phrase{};
  phrase.barCount = 1;
  uint8_t eventSteps[kStepsPerBar]{};
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((onsetMask & stepBit(step)) == 0) continue;
    const uint16_t index = phrase.eventCount++;
    phrase.events[index].role = role;
    phrase.events[index].idealTick =
        static_cast<uint16_t>(step * kFeelTicksPerStep);
    phrase.events[index].durationTicks = kFeelTicksPerStep;
    eventSteps[index] = step;
  }

  FeelInterpretRequest request{};
  request.phrase = &phrase;
  request.profile = profile;
  request.amount = amount;
  request.generation = generation;
  TimedFeelPhrase timed{};
  const FeelInterpretStatus status = interpretFeelPhrase(request, timed);
  if (status != FeelInterpretStatus::Ok) return status;

  SynthPattern next = destination;
  for (uint16_t index = 0; index < timed.eventCount; ++index) {
    next.steps[eventSteps[index]].timing = timed.events[index].offsetTicks;
  }
  destination = next;
  return FeelInterpretStatus::Ok;
}

}  // namespace GroovePuterRhythm
