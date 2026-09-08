#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_DURATION_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_DURATION_EDIT_H

#include <cstdint>

#include "phrase_notes_selection.h"
#include "src/phrase/runtime_phrase_edit.h"

// UI policy adapter for GRID-sized duration edits. The resolved selected event
// is the preferred target; GRID determines the edit increment, not event
// identity. Cursor-time targeting remains as a compatibility entry point.
//
// This is not a musical-state owner. RuntimePhraseEdit remains authoritative;
// audio/control exclusion remains the caller's job.
namespace PhraseNotesDurationEdit {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct Prepared {
  Buffer before{};
  Buffer after{};
};

enum class Result : uint8_t {
  Ready = 0,
  NoTarget,
  Rejected,
};

inline Result prepareSelected(const Buffer& live,
                              uint16_t eventIndex,
                              RuntimePhraseEdit::Grid grid,
                              int direction,
                              Prepared& out) {
  out.before = live;
  out.after = live;

  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;
  if (eventIndex >= live.count) return Result::NoTarget;

  RuntimePhraseEdit::EventEditResult editResult =
      RuntimePhraseEdit::EventEditResult::Rejected;
  const RuntimePhraseEdit::PrepareResult prepareResult =
      RuntimePhraseEdit::prepare(
          live, out.after, [&](Buffer& candidate) {
            editResult = RuntimePhraseEdit::resizeEventByGrid(
                candidate, eventIndex, direction, grid);
          });

  if (editResult != RuntimePhraseEdit::EventEditResult::Changed ||
      prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    return Result::Rejected;
  }
  return Result::Ready;
}

inline Result prepare(const Buffer& live,
                      uint16_t cursorTick,
                      RuntimePhraseEdit::Grid grid,
                      int direction,
                      Prepared& out) {
  if (!RuntimePhraseEdit::validate(live)) {
    out.before = live;
    out.after = live;
    return Result::Rejected;
  }
  const PhraseNotesSelection::Selection selection =
      PhraseNotesSelection::derive(live, cursorTick);
  if (!selection.active) {
    out.before = live;
    out.after = live;
    return Result::NoTarget;
  }
  return prepareSelected(live, selection.eventIndex, grid, direction, out);
}

inline bool commitIfUnchanged(Buffer& live, const Prepared& prepared) {
  if (!RuntimePhraseEdit::same(live, prepared.before)) return false;
  return RuntimePhraseEdit::commit(live, prepared.after);
}

}  // namespace PhraseNotesDurationEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_DURATION_EDIT_H
