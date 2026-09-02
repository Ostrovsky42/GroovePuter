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

  // Every fallible operation has completed. destination is still a local
  // materialization candidate, so write its timing in place instead of making
  // a second full pattern copy on the constrained Cardputer UI stack.
  for (uint16_t i = 0; i < timed.eventCount; ++i) {
    const FeelPhraseEvent& source = phrase.events[i];
    const int8_t drumVoice = binding.drumVoiceByRole[
        static_cast<uint8_t>(source.role)];
    const uint8_t step = static_cast<uint8_t>(
        source.idealTick / kFeelTicksPerStep);
    destination.drums.voices[drumVoice].steps[step].timing =
        timed.events[i].offsetTicks;
  }
  return FeelPatternApplyStatus::Ok;
}

}  // namespace GroovePuterRhythm
