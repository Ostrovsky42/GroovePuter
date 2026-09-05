#pragma once

#include <cstdint>

#include "../phrase/runtime_phrase_edit.h"

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

inline bool validate(const PhraseRuntime::RuntimeSynthEventBuffer& phrase) {
  return RuntimePhraseEdit::validate(phrase);
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
