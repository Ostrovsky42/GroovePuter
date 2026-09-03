#include "runtime_synth_events.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace PhraseRuntime {
namespace {

struct TriggerToken {
  uint16_t tick = 0;
  uint8_t stepIndex = 0;
  int8_t note = -1;
};

struct TriggerTokenBuffer {
  TriggerToken values[SynthPattern::kSteps]{};
  uint8_t count = 0;
};

int clampSwingPercent(uint8_t value) {
  int swing = static_cast<int>(value);
  if (swing < 50) swing = 50;
  if (swing > 75) swing = 75;
  return swing;
}

int swingDelayTicks(const PatternProjectionSettings& settings) {
  const int swing = clampSwingPercent(settings.swingPercent);
  return static_cast<int>(
      std::lround((static_cast<float>(swing) - 50.0f) * 24.0f / 50.0f));
}

uint16_t baseGateDurationSubticks(const PatternProjectionSettings& settings) {
  float gate = settings.gateLengthRatio;
  if (!std::isfinite(gate) || gate < 0.1f) gate = 0.5f;

  float effective = gate * (settings.synthIndex == 0 ? 0.85f : 1.05f);
  if (settings.synthIndex == 0 && effective < 0.15f) effective = 0.15f;
  if (settings.synthIndex == 1 && effective > 0.98f) effective = 0.98f;

  long subticks = std::lround(
      24.0f * effective * static_cast<float>(kSubticksPerTick));
  if (subticks < 1) subticks = 1;
  if (subticks > static_cast<long>(std::numeric_limits<uint16_t>::max())) {
    subticks = static_cast<long>(std::numeric_limits<uint16_t>::max());
  }
  return static_cast<uint16_t>(subticks);
}

TriggerTokenBuffer collectTriggerTokens(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings) {
  TriggerTokenBuffer tokens{};
  const int swingDelay = swingDelayTicks(settings);

  for (uint16_t barTick = 0; barTick < kTicksPerBar; ++barTick) {
    const int nominalStep = static_cast<int>(barTick / 24u);
    for (int scanned = nominalStep - 1; scanned <= nominalStep + 1; ++scanned) {
      const int stepIndex = (scanned + SynthPattern::kSteps) % SynthPattern::kSteps;
      const SynthStep& step = pattern.steps[stepIndex];
      const int swing =
          settings.swingEnabled && ((stepIndex & 1) != 0) ? swingDelay : 0;
      const int nominalTick = stepIndex * 24;
      const int triggerTick =
          (nominalTick + swing + static_cast<int>(step.timing) + kTicksPerBar) %
          kTicksPerBar;
      if (triggerTick != static_cast<int>(barTick)) continue;
      if (step.note < -2) continue;
      if (step.note == -1) continue;
      if (tokens.count >= SynthPattern::kSteps) continue;

      TriggerToken& token = tokens.values[tokens.count++];
      token.tick = barTick;
      token.stepIndex = static_cast<uint8_t>(stepIndex);
      token.note = step.note;
    }
  }
  return tokens;
}

uint8_t eventFlags(const SynthStep& step) {
  uint8_t flags = 0;
  if (step.accent) flags |= kEventAccent;
  if (step.slide) flags |= kEventSlide;
  if (step.ghost) flags |= kEventGhost;
  return flags;
}

uint32_t absoluteTokenSubtick(const TriggerTokenBuffer& tokens,
                              uint8_t originIndex,
                              uint8_t tokenIndex) {
  uint32_t tick = tokens.values[tokenIndex].tick;
  if (tokenIndex <= originIndex) tick += kTicksPerBar;
  return tick * kSubticksPerTick;
}

void foldLegacyLifetime(const TriggerTokenBuffer& tokens,
                        uint8_t originIndex,
                        uint16_t baseDuration,
                        RuntimeSynthEvent& event) {
  const uint32_t start =
      static_cast<uint32_t>(event.startTick) * kSubticksPerTick;
  uint32_t end = start + baseDuration;

  if (tokens.count == 0) {
    event.durationSubticks = baseDuration;
    return;
  }

  for (uint8_t offset = 1; offset <= tokens.count; ++offset) {
    const uint8_t tokenIndex =
        static_cast<uint8_t>((originIndex + offset) % tokens.count);
    const TriggerToken& token = tokens.values[tokenIndex];
    const uint32_t tokenTime =
        absoluteTokenSubtick(tokens, originIndex, tokenIndex);

    if (token.note >= 0) {
      if (tokenTime < end) end = tokenTime;
      break;
    }

    if (token.note == -2 && tokenTime <= end) {
      end += baseDuration;
    }
  }

  uint32_t duration = end > start ? end - start : 1u;
  if (duration > std::numeric_limits<uint16_t>::max()) {
    duration = std::numeric_limits<uint16_t>::max();
  }
  event.durationSubticks = static_cast<uint16_t>(duration);
}

}  // namespace

PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination) {
  if (settings.synthIndex >= 2) {
    return PatternProjectionStatus::InvalidSynthIndex;
  }

  RuntimeSynthEventBuffer candidate{};
  candidate.lengthTicks = kTicksPerBar;

  const TriggerTokenBuffer tokens = collectTriggerTokens(pattern, settings);
  const uint16_t baseDuration = baseGateDurationSubticks(settings);

  uint8_t eventTokenIndices[SynthPattern::kSteps]{};
  for (uint8_t tokenIndex = 0; tokenIndex < tokens.count; ++tokenIndex) {
    const TriggerToken& token = tokens.values[tokenIndex];
    if (token.note < 0) continue;
    if (candidate.count >= kMaxSynthEvents) break;

    const SynthStep& step = pattern.steps[token.stepIndex];
    RuntimeSynthEvent& event = candidate.events[candidate.count];
    event.startTick = token.tick;
    event.durationSubticks = baseDuration;
    event.note = static_cast<uint8_t>(token.note);
    event.velocity = step.velocity;
    event.probability = step.probability;
    event.flags = eventFlags(step);
    event.fx = step.fx;
    event.fxParam = step.fxParam;
    eventTokenIndices[candidate.count] = tokenIndex;
    ++candidate.count;
  }

  for (uint16_t eventIndex = 0; eventIndex < candidate.count; ++eventIndex) {
    foldLegacyLifetime(
        tokens,
        eventTokenIndices[eventIndex],
        baseDuration,
        candidate.events[eventIndex]);
  }

  destination = candidate;
  return PatternProjectionStatus::Ready;
}

}  // namespace PhraseRuntime
