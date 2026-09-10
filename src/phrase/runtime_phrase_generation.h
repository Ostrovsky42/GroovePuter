#pragma once
#ifndef GROOVEPUTER_PHRASE_RUNTIME_PHRASE_GENERATION_H
#define GROOVEPUTER_PHRASE_RUNTIME_PHRASE_GENERATION_H

#include <cstdint>

#include "runtime_synth_events.h"

namespace PhraseGeneration {

enum class Result : uint8_t {
  Ready = 0,
  InvalidLength,
  ProjectionFailed,
  Empty,
  CapacityExceeded,
};

// Turn an already-generated one-bar SynthPattern into Phrase-owned runtime
// material without ever committing that pattern to Scene/Pattern storage.
// 0.9.11 only promises deterministic REPEAT when material spans more than one
// bar, so the generated idea is repeated over the Phrase's existing extent.
inline Result projectRepeatedPattern(
    const SynthPattern& generatedPattern,
    const PhraseRuntime::PatternProjectionSettings& settings,
    uint16_t phraseLengthTicks,
    PhraseRuntime::RuntimeSynthEventBuffer& destination) {
  if (phraseLengthTicks == 0 ||
      phraseLengthTicks % PhraseRuntime::kTicksPerBar != 0 ||
      phraseLengthTicks > PhraseRuntime::kTicksPerBar *
                              PhraseRuntime::kMaxPhraseBars) {
    return Result::InvalidLength;
  }

  PhraseRuntime::RuntimeSynthEventBuffer oneBar{};
  if (PhraseRuntime::projectPatternToRuntimeEvents(
          generatedPattern, settings, oneBar) !=
      PhraseRuntime::PatternProjectionStatus::Ready) {
    return Result::ProjectionFailed;
  }
  if (oneBar.count == 0) return Result::Empty;

  const uint16_t bars = static_cast<uint16_t>(
      phraseLengthTicks / PhraseRuntime::kTicksPerBar);
  const uint32_t totalEvents =
      static_cast<uint32_t>(oneBar.count) * static_cast<uint32_t>(bars);
  if (totalEvents > PhraseRuntime::kMaxSynthEvents) {
    return Result::CapacityExceeded;
  }

  PhraseRuntime::RuntimeSynthEventBuffer candidate{};
  candidate.lengthTicks = phraseLengthTicks;
  for (uint16_t bar = 0; bar < bars; ++bar) {
    const uint16_t offset = static_cast<uint16_t>(
        bar * PhraseRuntime::kTicksPerBar);
    for (uint16_t i = 0; i < oneBar.count; ++i) {
      PhraseRuntime::RuntimeSynthEvent event = oneBar.events[i];
      event.startTick = static_cast<uint16_t>(event.startTick + offset);
      candidate.events[candidate.count++] = event;
    }
  }

  destination = candidate;
  return Result::Ready;
}

}  // namespace PhraseGeneration

#endif  // GROOVEPUTER_PHRASE_RUNTIME_PHRASE_GENERATION_H
