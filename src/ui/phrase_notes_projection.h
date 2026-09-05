#pragma once

#include <cstdint>

#include "../phrase/runtime_synth_events.h"

namespace PhraseNotesProjection {

struct NoteSpan {
  uint16_t eventIndex = 0;
  uint16_t startTick = 0;
  uint16_t endTick = 0;
  uint32_t startSubtick = 0;
  uint32_t endSubtick = 0;
  uint8_t note = 0;
  bool crossesBarBoundary = false;
};

inline bool validPhraseLengthTicks(uint16_t lengthTicks) {
  return lengthTicks == PhraseRuntime::kTicksPerBar ||
         lengthTicks == 2 * PhraseRuntime::kTicksPerBar ||
         lengthTicks == 4 * PhraseRuntime::kTicksPerBar ||
         lengthTicks == 8 * PhraseRuntime::kTicksPerBar;
}

inline bool validate(const PhraseRuntime::RuntimeSynthEventBuffer& phrase) {
  if (!validPhraseLengthTicks(phrase.lengthTicks)) return false;
  if (phrase.count > PhraseRuntime::kMaxSynthEvents) return false;

  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(phrase.lengthTicks) * PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    const auto& event = phrase.events[i];
    if (event.durationSubticks == 0) return false;
    if (event.startTick >= phrase.lengthTicks) return false;
    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) * PhraseRuntime::kSubticksPerTick;
    const uint32_t endSubtick = startSubtick + event.durationSubticks;
    if (endSubtick > phraseEndSubtick) return false;
  }
  return true;
}

inline bool project(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                    uint16_t eventIndex,
                    NoteSpan& out) {
  if (!validate(phrase) || eventIndex >= phrase.count) return false;

  const auto& event = phrase.events[eventIndex];
  const uint32_t startSubtick =
      static_cast<uint32_t>(event.startTick) * PhraseRuntime::kSubticksPerTick;
  const uint32_t endSubtick = startSubtick + event.durationSubticks;
  const uint32_t barSubticks =
      static_cast<uint32_t>(PhraseRuntime::kTicksPerBar) *
      PhraseRuntime::kSubticksPerTick;

  out.eventIndex = eventIndex;
  out.startTick = event.startTick;
  out.endTick = static_cast<uint16_t>(
      (endSubtick + PhraseRuntime::kSubticksPerTick - 1u) /
      PhraseRuntime::kSubticksPerTick);
  out.startSubtick = startSubtick;
  out.endSubtick = endSubtick;
  out.note = event.note;
  out.crossesBarBoundary =
      (startSubtick / barSubticks) != ((endSubtick - 1u) / barSubticks);
  return true;
}

}  // namespace PhraseNotesProjection
