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

int triggerTickForStep(const SynthPattern& pattern,
                       const PatternProjectionSettings& settings,
                       uint8_t stepIndex) {
  const int swing = settings.swingEnabled && ((stepIndex & 1u) != 0u)
      ? swingDelayTicks(settings)
      : 0;
  const int nominalTick = static_cast<int>(stepIndex) * 24;
  return (nominalTick + swing +
          static_cast<int>(pattern.steps[stepIndex].timing) + kTicksPerBar) %
         kTicksPerBar;
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

  for (uint16_t barTick = 0; barTick < kTicksPerBar; ++barTick) {
    const int nominalStep = static_cast<int>(barTick / 24u);
    for (int scanned = nominalStep - 1; scanned <= nominalStep + 1; ++scanned) {
      const int stepIndex = (scanned + SynthPattern::kSteps) % SynthPattern::kSteps;
      const SynthStep& step = pattern.steps[stepIndex];
      const int triggerTick = triggerTickForStep(
          pattern, settings, static_cast<uint8_t>(stepIndex));
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

bool isGuaranteedOnset(const SynthPattern& pattern,
                       const TriggerToken& token) {
  if (token.note < 0) return false;
  const SynthStep& step = pattern.steps[token.stepIndex];
  // Keep projection pure. Legacy runtime considers the onset unconditional only
  // when ghost cannot reject it and probability cannot consume an RNG draw.
  return !step.ghost && step.probability >= 100;
}

void foldLegacyLifetime(const SynthPattern& pattern,
                        const PatternProjectionSettings& settings,
                        const TriggerTokenBuffer& tokens,
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

  // Step that currently holds this lifetime: the origin, then any TIE that
  // extended it. Slide legato is only defined into the immediately next step.
  uint8_t holdingStep = tokens.values[originIndex].stepIndex;

  for (uint8_t offset = 1; offset <= tokens.count; ++offset) {
    const uint8_t tokenIndex =
        static_cast<uint8_t>((originIndex + offset) % tokens.count);
    const TriggerToken& token = tokens.values[tokenIndex];
    const uint32_t tokenTime =
        absoluteTokenSubtick(tokens, originIndex, tokenIndex);
    const bool nextStep =
        token.stepIndex ==
        static_cast<uint8_t>((holdingStep + 1u) % SynthPattern::kSteps);

    if (token.note >= 0) {
      // TB-303 slide: the gate of the preceding note stays high until the
      // sliding note starts, so the common owner emits Release -> Start in
      // one batch and the voice glides legato instead of re-attacking. The
      // gate length alone is always shorter than a step, so without this the
      // slide flag could never take effect.
      if (nextStep && pattern.steps[token.stepIndex].slide && tokenTime > end) {
        end = tokenTime;
      }

      // A note token is not necessarily a sounding onset. Ghost/probability are
      // resolved later by the runtime executor in their legacy RNG order. If
      // it lies strictly after expiry, nothing later may resurrect this note.
      // At the exact boundary a conditional onset may be rejected; keep
      // scanning so a following adjacent TIE can preserve the current owner.
      if (tokenTime > end) break;

      if (isGuaranteedOnset(pattern, token)) {
        // Guaranteed future onset will definitely replace the old monophonic
        // lifetime, so deterministic pre-clipping is behavior-equivalent.
        end = tokenTime;
        break;
      }

      // Conditional onset may be rejected at runtime. Keep the old lifetime
      // alive and continue scanning so a later TIE can still extend it. If the
      // onset is accepted, the common P2 owner performs Release -> Start there.
      holdingStep = token.stepIndex;
      continue;
    }

    if (token.note == -2) {
      if (nextStep) {
        // TIE is a continuation of the immediately preceding source step, not
        // a new onset. Keep the current pitch owned through the tied step and
        // release at the following source-step boundary. This intentionally
        // extends short gates whose natural deadline precedes the TIE token.
        const int nextTick = triggerTickForStep(
            pattern,
            settings,
            static_cast<uint8_t>((token.stepIndex + 1u) % SynthPattern::kSteps));
        int ticksUntilNextStep = nextTick - static_cast<int>(token.tick);
        if (ticksUntilNextStep <= 0) ticksUntilNextStep += kTicksPerBar;
        const uint32_t tieEnd = tokenTime +
            static_cast<uint32_t>(ticksUntilNextStep) * kSubticksPerTick;
        if (tieEnd > end) end = tieEnd;
        holdingStep = token.stepIndex;
      } else if (tokenTime >= end) {
        // A non-adjacent TIE cannot bridge a REST or revive an expired note.
        break;
      }
    }
  }

  uint32_t duration = end > start ? end - start : 1u;
  if (duration > std::numeric_limits<uint16_t>::max()) {
    duration = std::numeric_limits<uint16_t>::max();
  }
  event.durationSubticks = static_cast<uint16_t>(duration);
}

}  // namespace

namespace {

PatternProjectionStatus projectPatternToRuntimeEventsImpl(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination,
    uint8_t* sourceSteps) {
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
    if (sourceSteps != nullptr && candidate.count < SynthPattern::kSteps) {
      sourceSteps[candidate.count] = token.stepIndex;
    }
    ++candidate.count;
  }

  for (uint16_t eventIndex = 0; eventIndex < candidate.count; ++eventIndex) {
    foldLegacyLifetime(
        pattern,
        settings,
        tokens,
        eventTokenIndices[eventIndex],
        baseDuration,
        candidate.events[eventIndex]);
  }

  destination = candidate;
  return PatternProjectionStatus::Ready;
}

}  // namespace

PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination) {
  return projectPatternToRuntimeEventsImpl(
      pattern, settings, destination, nullptr);
}

PatternProjectionStatus projectPatternToRuntimeEventsWithSourceSteps(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination,
    uint8_t (&sourceSteps)[SynthPattern::kSteps]) {
  for (uint8_t& sourceStep : sourceSteps) {
    sourceStep = 0xFFu;
  }
  return projectPatternToRuntimeEventsImpl(
      pattern, settings, destination, sourceSteps);
}

}  // namespace PhraseRuntime
