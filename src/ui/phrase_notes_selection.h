#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_SELECTION_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_SELECTION_H

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

// Derive from the cursor *cell*, not from the single tick at its left edge.
//
// The cursor addresses grid cells, but selection used the exact tick, so an
// onset that does not sit on the grid could not be selected at all: a note
// starting at tick 100 with a 1/16 grid put the cursor at 96, and 96 lies
// before the note. Projected material is full of such onsets -- swing and
// micro-timing move them off the grid by construction -- so "select the next
// sound" silently selected nothing.
//
// An onset inside the cell wins over a note merely passing through it, because
// the cell is where the user just landed and the onset is what they aimed at.
inline Selection deriveInCell(
    const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
    uint16_t cursorTick,
    uint16_t cellTicks) {
  if (cellTicks > 0) {
    int best = -1;
    uint16_t bestStart = 0;
    for (uint16_t i = 0; i < phrase.count; ++i) {
      const uint16_t start = phrase.events[i].startTick;
      if (start < cursorTick || start >= cursorTick + cellTicks) continue;
      if (best < 0 || start < bestStart) {
        best = static_cast<int>(i);
        bestStart = start;
      }
    }
    if (best >= 0) {
      PhraseNotesProjection::NoteSpan span{};
      if (PhraseNotesProjection::project(
              phrase, static_cast<uint16_t>(best), span)) {
        Selection out{};
        out.active = true;
        out.eventIndex = static_cast<uint16_t>(best);
        out.span = span;
        return out;
      }
    }
  }
  return derive(phrase, cursorTick);
}

}  // namespace PhraseNotesSelection

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_SELECTION_H
