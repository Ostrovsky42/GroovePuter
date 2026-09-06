#pragma once

#include <cstdint>

#include "phrase_notes_selection.h"
#include "src/phrase/runtime_phrase_edit.h"

// U4B4 UI policy adapter: derive the deletion target from cursor-time truth,
// prepare a complete bounded before/after value, and allow commit only while the
// live Phrase still matches the prepared before-image.
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

inline Result prepare(const Buffer& live,
                      uint16_t cursorTick,
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
            editResult = RuntimePhraseEdit::deleteEvent(
                candidate, selection.eventIndex);
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

}  // namespace PhraseNotesDeleteEdit
