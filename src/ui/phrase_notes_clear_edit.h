#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_CLEAR_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_CLEAR_EDIT_H

#include <cstdint>

#include "src/phrase/runtime_phrase_edit.h"

// UI policy adapter for "clear the whole melody": every sound goes, the melody length stays.
// Same shape as its delete/insert siblings: a bounded before-image the caller commits with Undo.
namespace PhraseNotesClearEdit {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct Prepared {
  Buffer before{};
  Buffer after{};
};

enum class Result : uint8_t {
  Ready = 0,
  NothingToClear,
  Rejected,
};

inline Result prepare(const Buffer& live, Prepared& out) {
  out.before = live;
  out.after = live;
  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;
  if (live.count == 0) return Result::NothingToClear;

  out.after = Buffer{};
  out.after.lengthTicks = live.lengthTicks;
  if (!RuntimePhraseEdit::validate(out.after)) {
    out.after = live;
    return Result::Rejected;
  }
  return Result::Ready;
}

}  // namespace PhraseNotesClearEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_CLEAR_EDIT_H
