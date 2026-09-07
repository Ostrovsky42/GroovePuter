#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_PITCH_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_PITCH_EDIT_H

#include <cstdint>

#include "phrase_notes_selection.h"
#include "src/phrase/runtime_phrase_edit.h"

// U4B7 UI policy adapter: derive the target from cursor-time truth, prepare one
// semitone change on a bounded before-image, then allow the caller to commit
// only if the live Phrase is still the value that was prepared.
//
// Same shape and same obligations as PhraseNotesDurationEdit (U4B3): this is
// not a new musical-state owner, RuntimePhraseEdit remains the authoritative
// mutation primitive, and audio/control exclusion remains the caller's job.
//
// One press is one semitone because the beginner screen labels these keys
// "выше" and "ниже" and nothing else. A scale-aware step would make the same
// key produce different intervals depending on state the screen does not show.
namespace PhraseNotesPitchEdit {

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

inline Result prepare(const Buffer& live,
                      uint16_t cursorTick,
                      int direction,
                      Prepared& out) {
  out.before = live;
  out.after = live;

  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;
  const PhraseNotesSelection::Selection selection =
      PhraseNotesSelection::derive(live, cursorTick);
  if (!selection.active) return Result::NoTarget;

  RuntimePhraseEdit::EventEditResult editResult =
      RuntimePhraseEdit::EventEditResult::Rejected;
  const RuntimePhraseEdit::PrepareResult prepareResult =
      RuntimePhraseEdit::prepare(
          live, out.after, [&](Buffer& candidate) {
            editResult = RuntimePhraseEdit::transposeEvent(
                candidate, selection.eventIndex, direction);
          });

  if (editResult != RuntimePhraseEdit::EventEditResult::Changed ||
      prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    return Result::Rejected;
  }
  return Result::Ready;
}

inline bool commitIfUnchanged(Buffer& live, const Prepared& prepared) {
  if (!RuntimePhraseEdit::same(live, prepared.before)) return false;
  return RuntimePhraseEdit::commit(live, prepared.after);
}

}  // namespace PhraseNotesPitchEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_PITCH_EDIT_H
