#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_JOIN_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_JOIN_EDIT_H

#include <cstdint>

#include "phrase_notes_selection.h"
#include "src/phrase/runtime_phrase_edit.h"

// U4C2 UI policy adapter: "continue instead of the next sound".
//
// Same shape and obligations as its siblings: RuntimePhraseEdit owns the
// mutation, the caller owns audio/control exclusion, and the whole change is
// one prepared value so a single undo restores both sounds.
namespace PhraseNotesJoinEdit {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct Prepared {
  Buffer before{};
  Buffer after{};
};

enum class Result : uint8_t {
  Ready = 0,
  NoTarget,
  NoNext,
  Ambiguous,
  Rejected,
};

inline Result prepare(const Buffer& live,
                      uint16_t cursorTick,
                      RuntimePhraseEdit::Grid grid,
                      Prepared& out) {
  out.before = live;
  out.after = live;

  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;
  const PhraseNotesSelection::Selection selection =
      PhraseNotesSelection::deriveInCell(
          live, cursorTick, RuntimePhraseEdit::gridTicks(grid));
  if (!selection.active) return Result::NoTarget;

  RuntimePhraseEdit::JoinResult joinResult =
      RuntimePhraseEdit::JoinResult::Rejected;
  const RuntimePhraseEdit::PrepareResult prepareResult =
      RuntimePhraseEdit::prepare(
          live, out.after, [&](Buffer& candidate) {
            joinResult =
                RuntimePhraseEdit::joinNextEvent(candidate, selection.eventIndex);
          });

  if (joinResult != RuntimePhraseEdit::JoinResult::Changed ||
      prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    switch (joinResult) {
      case RuntimePhraseEdit::JoinResult::NoNext: return Result::NoNext;
      case RuntimePhraseEdit::JoinResult::NoTarget: return Result::NoTarget;
      case RuntimePhraseEdit::JoinResult::Ambiguous: return Result::Ambiguous;
      default: return Result::Rejected;
    }
  }
  return Result::Ready;
}

inline bool commitIfUnchanged(Buffer& live, const Prepared& prepared) {
  if (!RuntimePhraseEdit::same(live, prepared.before)) return false;
  return RuntimePhraseEdit::commit(live, prepared.after);
}

}  // namespace PhraseNotesJoinEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_JOIN_EDIT_H
