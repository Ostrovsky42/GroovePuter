#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_DELETE_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_DELETE_EDIT_H

#include <cstdint>

#include "phrase_notes_selection.h"
#include "src/phrase/runtime_phrase_edit.h"

// UI policy adapter for deletion. The resolved selected event is the preferred
// target because cursor time cannot identify coincident events. Cursor-time
// targeting remains as a compatibility entry point.
//
// RuntimePhraseEdit remains the mutation owner; audio/control exclusion remains
// the caller's responsibility.
namespace PhraseNotesDeleteEdit {

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
            editResult = RuntimePhraseEdit::deleteEvent(candidate, eventIndex);
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
  return prepareSelected(live, selection.eventIndex, out);
}

inline bool commitIfUnchanged(Buffer& live, const Prepared& prepared) {
  if (!RuntimePhraseEdit::same(live, prepared.before)) return false;
  return RuntimePhraseEdit::commit(live, prepared.after);
}

}  // namespace PhraseNotesDeleteEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_DELETE_EDIT_H
