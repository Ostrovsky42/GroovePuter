#pragma once

#include <cstdint>

#include "phrase_notes_projection.h"

namespace PhraseNotesSelection {

struct Selection {
  bool active = false;
  uint16_t eventIndex = 0;
  PhraseNotesProjection::NoteSpan span{};
};

inline Selection derive(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                        uint16_t cursorTick) {
  Selection out{};
  const int eventIndex = RuntimePhraseEdit::eventCoveringTick(phrase, cursorTick);
  if (eventIndex < 0) return out;

  PhraseNotesProjection::NoteSpan span{};
  if (!PhraseNotesProjection::project(
          phrase, static_cast<uint16_t>(eventIndex), span)) {
    return out;
  }

  out.active = true;
  out.eventIndex = static_cast<uint16_t>(eventIndex);
  out.span = span;
  return out;
}

}  // namespace PhraseNotesSelection
