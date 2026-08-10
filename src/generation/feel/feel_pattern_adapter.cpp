#include "feel_pattern_adapter.h"

namespace GroovePuterRhythm {
namespace {

StepMask allOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural | role.secondary | role.ghosts);
}

}  // namespace

FeelPatternApplyStatus applyFeelToMaterializedPattern(
    const RhythmPhrasePlan& plan,
    const PatternMaterializerBinding& binding,
    FeelProfileId profile,
    uint8_t amount,
    const GenerationContext& generation,
    MaterializedPatterns& destination) {
  if (plan.barCount != 1) return FeelPatternApplyStatus::InvalidPlan;

  FeelPhrase phrase{};
  phrase.barCount = 1;
  uint8_t eventRole[kMaxFeelEvents]{};
  uint8_t eventStep[kMaxFeelEvents]{};
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
      const int8_t drumVoice = binding.drumVoiceByRole[roleIndex];
      if (drumVoice < -1 ||
          drumVoice >= static_cast<int8_t>(DrumPatternSet::kVoices)) {
        return FeelPatternApplyStatus::InvalidBinding;
      }
      if (drumVoice < 0) continue;
      if ((allOnsets(plan.bars[0].roles[roleIndex]) & stepBit(step)) == 0) {
        continue;
      }
      if (phrase.eventCount >= kMaxFeelEvents) {
        return FeelPatternApplyStatus::InterpretFailed;
      }
      const uint16_t index = phrase.eventCount++;
      phrase.events[index].role = static_cast<RhythmRole>(roleIndex);
      phrase.events[index].idealTick =
          static_cast<uint16_t>(step * kFeelTicksPerStep);
      phrase.events[index].durationTicks = kFeelTicksPerStep;
      eventRole[index] = roleIndex;
      eventStep[index] = step;
    }
  }

  FeelInterpretRequest request{};
  request.phrase = &phrase;
  request.profile = profile;
  request.amount = amount;
  request.generation = generation;
  TimedFeelPhrase timed{};
  if (interpretFeelPhrase(request, timed) != FeelInterpretStatus::Ok) {
    return FeelPatternApplyStatus::InterpretFailed;
  }

  MaterializedPatterns next = destination;
  for (uint16_t i = 0; i < timed.eventCount; ++i) {
    const int8_t drumVoice = binding.drumVoiceByRole[eventRole[i]];
    next.drums.voices[drumVoice].steps[eventStep[i]].timing =
        timed.events[i].offsetTicks;
  }
  destination = next;
  return FeelPatternApplyStatus::Ok;
}

}  // namespace GroovePuterRhythm
