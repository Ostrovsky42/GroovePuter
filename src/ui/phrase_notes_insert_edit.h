#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_INSERT_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_INSERT_EDIT_H

#include <cstdint>

#include "src/phrase/runtime_phrase_edit.h"

// U4B8 UI policy adapter: add one sound at the cursor, snapped to the active
// grid, on a bounded before-image the caller then commits.
//
// Same shape and obligations as its siblings (U4B3 duration, U4B4 delete,
// U4B7 pitch): RuntimePhraseEdit stays the authoritative mutation primitive
// and audio/control exclusion remains the caller's job.
//
// The one policy decision that lives here is which pitch a new sound gets.
// Silence is not an option and a dialog is worse, so a new sound continues the
// melody: it takes the pitch of the nearest sound starting before the cursor,
// and falls back to middle C when there is nothing to continue. That rule is
// deterministic, needs no theory to predict, and Up/Down corrects it at once.
namespace PhraseNotesInsertEdit {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

constexpr uint8_t kFallbackNote = 60;   // middle C
constexpr uint8_t kInsertVelocity = 100;

struct Prepared {
  Buffer before{};
  Buffer after{};
};

enum class Result : uint8_t {
  Ready = 0,
  Occupied,
  Full,
  Rejected,
};

// The pitch to continue from: the latest onset at or before the target, or the
// fallback when the melody has nothing before that point.
inline uint8_t continuedNote(const Buffer& phrase, uint16_t targetTick) {
  uint8_t note = kFallbackNote;
  bool found = false;
  uint16_t bestStart = 0;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    const uint16_t start = phrase.events[i].startTick;
    if (start > targetTick) continue;
    if (!found || start >= bestStart) {
      bestStart = start;
      note = phrase.events[i].note;
      found = true;
    }
  }
  return note;
}

inline Result prepare(const Buffer& live,
                      uint16_t cursorTick,
                      RuntimePhraseEdit::Grid grid,
                      Prepared& out) {
  out.before = live;
  out.after = live;

  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;

  const uint16_t gridTicks = RuntimePhraseEdit::gridTicks(grid);
  if (gridTicks == 0) return Result::Rejected;
  const uint16_t snappedTick =
      static_cast<uint16_t>((cursorTick / gridTicks) * gridTicks);

  // Reported before attempting the edit so the caller can say *why* nothing
  // happened. Two events on one tick are a state the player resolves by taking
  // the first, so creating one here would hide a sound.
  for (uint16_t i = 0; i < live.count; ++i) {
    if (live.events[i].startTick == snappedTick) return Result::Occupied;
  }
  if (live.count >= PhraseRuntime::kMaxSynthEvents) return Result::Full;

  RuntimePhraseEdit::EventEditResult editResult =
      RuntimePhraseEdit::EventEditResult::Rejected;
  const RuntimePhraseEdit::PrepareResult prepareResult =
      RuntimePhraseEdit::prepare(
          live, out.after, [&](Buffer& candidate) {
            editResult = RuntimePhraseEdit::insertSnapped(
                candidate, cursorTick, gridTicks,
                continuedNote(live, snappedTick), kInsertVelocity);
          });

  if (editResult != RuntimePhraseEdit::EventEditResult::Changed ||
      prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    return editResult == RuntimePhraseEdit::EventEditResult::CapacityFull
        ? Result::Full
        : Result::Rejected;
  }
  return Result::Ready;
}

inline bool commitIfUnchanged(Buffer& live, const Prepared& prepared) {
  if (!RuntimePhraseEdit::same(live, prepared.before)) return false;
  return RuntimePhraseEdit::commit(live, prepared.after);
}

}  // namespace PhraseNotesInsertEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_INSERT_EDIT_H
