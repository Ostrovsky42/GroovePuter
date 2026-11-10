#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_NOTES_PITCH_EDIT_H
#define GROOVEPUTER_SRC_UI_PHRASE_NOTES_PITCH_EDIT_H

#include <cstdint>

#include "phrase_notes_selection.h"
#include "src/phrase/runtime_phrase_edit.h"

// UI policy adapter for one-semitone pitch edits. The editor's resolved
// selected-event identity is the preferred target because two sounds may share
// a start tick and an event may sit off the current GRID. Cursor-time targeting
// remains as a compatibility entry point for callers that do not yet own an
// explicit selection.
//
// This is not a musical-state owner: RuntimePhraseEdit remains authoritative
// and audio/control exclusion remains the caller's job.
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

inline Result prepareSelected(const Buffer& live,
                              uint16_t eventIndex,
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
            editResult = RuntimePhraseEdit::transposeEvent(
                candidate, eventIndex, direction);
          });

  if (editResult != RuntimePhraseEdit::EventEditResult::Changed ||
      prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    return Result::Rejected;
  }
  return Result::Ready;
}

// Several one-semitone steps as one edit (one undo): a step along the project
// key can be a whole tone or more. Each step keeps the one-semitone contract.
inline Result prepareSelectedBy(const Buffer& live,
                                uint16_t eventIndex,
                                int semitones,
                                Prepared& out) {
  out.before = live;
  out.after = live;
  if (semitones == 0 || semitones > 12 || semitones < -12) {
    return Result::Rejected;
  }
  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;
  if (eventIndex >= live.count) return Result::NoTarget;

  const int direction = semitones > 0 ? 1 : -1;
  bool changed = true;
  const RuntimePhraseEdit::PrepareResult prepareResult =
      RuntimePhraseEdit::prepare(live, out.after, [&](Buffer& candidate) {
        for (int i = 0; i < semitones * direction && changed; ++i) {
          changed = RuntimePhraseEdit::transposeEvent(candidate, eventIndex,
                                                      direction) ==
                    RuntimePhraseEdit::EventEditResult::Changed;
        }
      });
  if (!changed || prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    return Result::Rejected;
  }
  return Result::Ready;
}

// Transpose every event starting in `bar` using `mapPitch(uint8_t note) -> int`
// as one atomic edit (Ctrl+A bar selection). If any event in the bar hits the
// MIDI boundary, the whole edit is rejected so interval relationships hold.
template <typename PitchMapper>
inline Result prepareBarMapped(const Buffer& live,
                               uint8_t bar,
                               PitchMapper&& mapPitch,
                               Prepared& out) {
  out.before = live;
  out.after = live;
  if (!RuntimePhraseEdit::validate(live)) return Result::Rejected;

  const uint32_t barStart =
      static_cast<uint32_t>(bar) * PhraseRuntime::kTicksPerBar;
  const uint32_t barEnd = barStart + PhraseRuntime::kTicksPerBar;
  uint16_t count = 0;
  for (uint16_t i = 0; i < live.count; ++i) {
    if (live.events[i].startTick >= barStart &&
        live.events[i].startTick < barEnd) {
      ++count;
    }
  }
  if (count == 0) return Result::NoTarget;

  bool allValid = true;
  const RuntimePhraseEdit::PrepareResult prepareResult =
      RuntimePhraseEdit::prepare(live, out.after, [&](Buffer& candidate) {
        for (uint16_t i = 0; i < candidate.count && allValid; ++i) {
          if (candidate.events[i].startTick < barStart ||
              candidate.events[i].startTick >= barEnd) {
            continue;
          }
          const int cur = static_cast<int>(candidate.events[i].note);
          const int next = mapPitch(candidate.events[i].note);
          const int semitones = next - cur;
          if (next < 0 || next > 127 || semitones == 0 ||
              semitones > 12 || semitones < -12) {
            allValid = false;
            break;
          }
          const int dir = semitones > 0 ? 1 : -1;
          for (int s = 0; s < semitones * dir && allValid; ++s) {
            allValid = RuntimePhraseEdit::transposeEvent(candidate, i, dir) ==
                       RuntimePhraseEdit::EventEditResult::Changed;
          }
        }
      });

  if (!allValid || prepareResult != RuntimePhraseEdit::PrepareResult::Ready) {
    out.after = live;
    return Result::Rejected;
  }
  return Result::Ready;
}

inline Result prepare(const Buffer& live,
                      uint16_t cursorTick,
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
  return prepareSelected(live, selection.eventIndex, direction, out);
}

inline bool commitIfUnchanged(Buffer& live, const Prepared& prepared) {
  if (!RuntimePhraseEdit::same(live, prepared.before)) return false;
  return RuntimePhraseEdit::commit(live, prepared.after);
}

}  // namespace PhraseNotesPitchEdit

#endif  // GROOVEPUTER_SRC_UI_PHRASE_NOTES_PITCH_EDIT_H
