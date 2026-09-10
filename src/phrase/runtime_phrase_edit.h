#pragma once
#ifndef GROOVEPUTER_SRC_PHRASE_RUNTIME_PHRASE_EDIT_H
#define GROOVEPUTER_SRC_PHRASE_RUNTIME_PHRASE_EDIT_H

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "runtime_synth_events.h"

// P3-U1 bounded mutation boundary for the session-only per-synth Phrase buffer.
//
// Musical edits must never mutate the live RuntimeSynthEventBuffer incrementally.
// Callers prepare a complete before-image copy, mutate the copy, validate it,
// and only then replace the live buffer in one commit step. The caller remains
// responsible for executing commit() inside the existing audio/control guard.
// This owner deliberately knows nothing about Scene persistence, Undo or note
// lifetime; those are separate established owners/checkpoints.
namespace RuntimePhraseEdit {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

static_assert(std::is_trivially_copyable<Buffer>::value,
              "runtime Phrase buffer must remain a bounded value object");

enum class PrepareResult : uint8_t {
  Ready = 0,
  NoChange,
  Rejected,
};

enum class EventEditResult : uint8_t {
  Changed = 0,
  NoTarget,
  CapacityFull,
  Rejected,
};

enum class LengthEditResult : uint8_t {
  Changed = 0,
  NoChange,
  Rejected,
};

enum class Grid : uint8_t {
  Eighth = 0,
  Sixteenth,
  ThirtySecond,
};

inline uint16_t gridTicks(Grid grid) {
  switch (grid) {
    case Grid::Eighth:
      return PhraseRuntime::kTicksPerBar / 8u;
    case Grid::Sixteenth:
      return PhraseRuntime::kTicksPerBar / 16u;
    case Grid::ThirtySecond:
      return PhraseRuntime::kTicksPerBar / 32u;
  }
  return 0;
}

inline uint16_t lengthTicksForBars(uint8_t bars) {
  switch (bars) {
    case 1:
      return PhraseRuntime::kTicksPerBar;
    case 2:
      return 2u * PhraseRuntime::kTicksPerBar;
    case 4:
      return 4u * PhraseRuntime::kTicksPerBar;
    case 8:
      return 8u * PhraseRuntime::kTicksPerBar;
    default:
      return 0;
  }
}

inline bool validLengthTicks(uint16_t lengthTicks) {
  return lengthTicks == lengthTicksForBars(1) ||
         lengthTicks == lengthTicksForBars(2) ||
         lengthTicks == lengthTicksForBars(4) ||
         lengthTicks == lengthTicksForBars(8);
}

inline bool validGridTicks(uint16_t gridTicksValue) {
  return gridTicksValue == gridTicks(Grid::Eighth) ||
         gridTicksValue == gridTicks(Grid::Sixteenth) ||
         gridTicksValue == gridTicks(Grid::ThirtySecond);
}

inline bool validEventValues(const PhraseRuntime::RuntimeSynthEvent& event) {
  return event.note <= 127u &&
         event.velocity >= 1u &&
         event.velocity <= 127u &&
         event.probability <= 100u;
}

inline bool validate(const Buffer& phrase) {
  if (!validLengthTicks(phrase.lengthTicks)) return false;
  if (phrase.count > PhraseRuntime::kMaxSynthEvents) return false;

  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(phrase.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    const PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    if (!validEventValues(event)) return false;
    if (event.durationSubticks == 0) return false;
    if (event.startTick >= phrase.lengthTicks) return false;

    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    const uint32_t endSubtick =
        startSubtick + static_cast<uint32_t>(event.durationSubticks);
    if (endSubtick > phraseEndSubtick) return false;
  }

  return true;
}

inline bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Buffer)) == 0;
}

template <typename Mutator>
PrepareResult prepare(const Buffer& live, Buffer& prepared, Mutator&& mutator) {
  prepared = live;
  if (!validate(live)) return PrepareResult::Rejected;

  std::forward<Mutator>(mutator)(prepared);

  if (!validate(prepared)) {
    prepared = live;
    return PrepareResult::Rejected;
  }
  if (same(prepared, live)) return PrepareResult::NoChange;
  return PrepareResult::Ready;
}

// Complete-buffer replacement only. No field-level live mutation is exposed by
// this owner. The caller must hold the existing audio/control mutation guard.
inline bool commit(Buffer& live, const Buffer& prepared) {
  if (!validate(prepared)) return false;
  live = prepared;
  return true;
}

inline LengthEditResult setLengthBars(Buffer& phrase, uint8_t bars) {
  if (!validate(phrase)) return LengthEditResult::Rejected;

  const uint16_t nextLengthTicks = lengthTicksForBars(bars);
  if (nextLengthTicks == 0) return LengthEditResult::Rejected;
  if (nextLengthTicks == phrase.lengthTicks) return LengthEditResult::NoChange;

  const uint32_t nextEndSubtick =
      static_cast<uint32_t>(nextLengthTicks) *
      PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    const PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    if (event.startTick >= nextLengthTicks) return LengthEditResult::Rejected;

    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    const uint32_t endSubtick =
        startSubtick + static_cast<uint32_t>(event.durationSubticks);
    if (endSubtick > nextEndSubtick) return LengthEditResult::Rejected;
  }

  phrase.lengthTicks = nextLengthTicks;
  return LengthEditResult::Changed;
}

inline EventEditResult insertSnapped(Buffer& phrase,
                                     uint16_t cursorTick,
                                     uint16_t gridTicksValue,
                                     uint8_t note,
                                     uint8_t velocity) {
  if (!validate(phrase) || !validGridTicks(gridTicksValue) ||
      note > 127u || velocity == 0u || velocity > 127u) {
    return EventEditResult::Rejected;
  }
  if (phrase.count >= PhraseRuntime::kMaxSynthEvents) {
    return EventEditResult::CapacityFull;
  }

  const uint16_t startTick = static_cast<uint16_t>(
      (cursorTick / gridTicksValue) * gridTicksValue);
  if (startTick >= phrase.lengthTicks) return EventEditResult::Rejected;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].startTick == startTick) {
      return EventEditResult::Rejected;
    }
  }

  const uint32_t durationSubticks =
      static_cast<uint32_t>(gridTicksValue) * PhraseRuntime::kSubticksPerTick;
  const uint32_t startSubtick =
      static_cast<uint32_t>(startTick) * PhraseRuntime::kSubticksPerTick;
  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(phrase.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;
  if (startSubtick + durationSubticks > phraseEndSubtick) {
    return EventEditResult::Rejected;
  }

  PhraseRuntime::RuntimeSynthEvent event{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(durationSubticks);
  event.note = note;
  event.velocity = velocity;
  event.probability = 100;

  phrase.events[phrase.count] = event;
  ++phrase.count;
  return EventEditResult::Changed;
}

// U4B7 pitch primitive. Deliberately shaped like resizeEventByGrid: a single
// step in one direction, so the caller cannot express an unbounded jump and the
// key press maps one-to-one onto one musical step.
//
// Pitch is orthogonal to time. This never touches startTick, durationSubticks
// or the buffer length, so it can never invalidate a phrase that was valid.
// The MIDI range boundary is rejected rather than clamped: a silent clamp would
// report success for a press that changed nothing.
inline EventEditResult transposeEvent(Buffer& phrase,
                                      uint16_t eventIndex,
                                      int direction) {
  if (!validate(phrase)) return EventEditResult::Rejected;
  if (eventIndex >= phrase.count) return EventEditResult::NoTarget;
  if (direction != -1 && direction != 1) return EventEditResult::Rejected;

  const int nextNote =
      static_cast<int>(phrase.events[eventIndex].note) + direction;
  if (nextNote < 0 || nextNote > 127) return EventEditResult::Rejected;

  phrase.events[eventIndex].note = static_cast<uint8_t>(nextNote);
  return EventEditResult::Changed;
}

inline EventEditResult deleteEvent(Buffer& phrase, uint16_t eventIndex) {
  if (!validate(phrase)) return EventEditResult::Rejected;
  if (eventIndex >= phrase.count) return EventEditResult::NoTarget;

  for (uint16_t i = eventIndex; i + 1u < phrase.count; ++i) {
    phrase.events[i] = phrase.events[i + 1u];
  }
  phrase.events[phrase.count - 1u] = PhraseRuntime::RuntimeSynthEvent{};
  --phrase.count;
  return EventEditResult::Changed;
}

// U4C2: continue this sound instead of the next one.
//
// A note cannot sound longer than the gap to the next attack, because the voice
// releases it there. Making it longer therefore means the next attack has to
// go. That is a musical decision, so it is its own operation rather than a
// hidden consequence of resizing -- and it is one commit, so one undo restores
// both sounds.
//
// The surviving sound is the selected one: its pitch, velocity and flags are
// kept and only its length grows, to the later of the two ends.
enum class JoinResult : uint8_t {
  Changed = 0,
  NoTarget,
  NoNext,
  Ambiguous,
  Rejected,
};

inline JoinResult joinNextEvent(Buffer& phrase, uint16_t eventIndex) {
  if (!validate(phrase)) return JoinResult::Rejected;
  if (eventIndex >= phrase.count) return JoinResult::NoTarget;

  const PhraseRuntime::RuntimeSynthEvent selected = phrase.events[eventIndex];

  int next = -1;
  uint16_t nextStart = 0;
  uint16_t coincident = 0;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (i == eventIndex) continue;
    const uint16_t start = phrase.events[i].startTick;
    if (start <= selected.startTick) continue;
    if (next < 0 || start < nextStart) {
      next = static_cast<int>(i);
      nextStart = start;
      coincident = 1;
    } else if (start == nextStart) {
      ++coincident;
    }
  }
  if (next < 0) return JoinResult::NoNext;

  // Removing one of two sounds that start together would leave the other still
  // cutting the note, so the command would look like it did nothing.
  if (coincident > 1) return JoinResult::Ambiguous;

  const uint32_t selectedEnd =
      static_cast<uint32_t>(selected.startTick) *
          PhraseRuntime::kSubticksPerTick +
      selected.durationSubticks;
  const PhraseRuntime::RuntimeSynthEvent& neighbour =
      phrase.events[static_cast<uint16_t>(next)];
  const uint32_t neighbourEnd =
      static_cast<uint32_t>(neighbour.startTick) *
          PhraseRuntime::kSubticksPerTick +
      neighbour.durationSubticks;

  const uint32_t end = selectedEnd > neighbourEnd ? selectedEnd : neighbourEnd;
  const uint32_t startSubtick =
      static_cast<uint32_t>(selected.startTick) *
      PhraseRuntime::kSubticksPerTick;
  if (end <= startSubtick) return JoinResult::Rejected;
  const uint32_t duration = end - startSubtick;
  if (duration > UINT16_MAX) return JoinResult::Rejected;

  phrase.events[eventIndex].durationSubticks = static_cast<uint16_t>(duration);
  if (deleteEvent(phrase, static_cast<uint16_t>(next)) !=
      EventEditResult::Changed) {
    return JoinResult::Rejected;
  }
  return JoinResult::Changed;
}

inline EventEditResult resizeEventByGrid(Buffer& phrase,
                                         uint16_t eventIndex,
                                         int direction,
                                         Grid grid) {
  if (!validate(phrase)) return EventEditResult::Rejected;
  if (eventIndex >= phrase.count) return EventEditResult::NoTarget;
  if (direction != -1 && direction != 1) return EventEditResult::Rejected;

  const uint16_t quantumTicks = gridTicks(grid);
  if (!validGridTicks(quantumTicks)) return EventEditResult::Rejected;
  const uint32_t quantumSubticks =
      static_cast<uint32_t>(quantumTicks) * PhraseRuntime::kSubticksPerTick;

  const PhraseRuntime::RuntimeSynthEvent& current = phrase.events[eventIndex];
  uint32_t nextDuration = current.durationSubticks;
  if (direction > 0) {
    nextDuration += quantumSubticks;
  } else {
    if (nextDuration <= quantumSubticks) return EventEditResult::Rejected;
    nextDuration -= quantumSubticks;
  }

  const uint32_t startSubtick =
      static_cast<uint32_t>(current.startTick) * PhraseRuntime::kSubticksPerTick;
  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(phrase.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;
  if (nextDuration == 0 ||
      nextDuration > UINT16_MAX ||
      startSubtick + nextDuration > phraseEndSubtick) {
    return EventEditResult::Rejected;
  }

  phrase.events[eventIndex].durationSubticks =
      static_cast<uint16_t>(nextDuration);
  return EventEditResult::Changed;
}

// Return the event whose sounding interval contains cursorTick. If overlaps are
// present, the most recent onset wins; equal-onset ties retain the lower event
// index. End boundaries are exclusive so a cursor exactly at note-off does not
// keep selecting the finished note.
inline int eventCoveringTick(const Buffer& phrase, uint16_t cursorTick) {
  if (!validate(phrase) || cursorTick >= phrase.lengthTicks) return -1;

  const uint32_t cursorSubtick =
      static_cast<uint32_t>(cursorTick) * PhraseRuntime::kSubticksPerTick;
  int bestIndex = -1;
  uint16_t bestStartTick = 0;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    const PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    const uint32_t endSubtick =
        startSubtick + static_cast<uint32_t>(event.durationSubticks);
    if (cursorSubtick < startSubtick || cursorSubtick >= endSubtick) continue;

    if (bestIndex < 0 || event.startTick > bestStartTick) {
      bestIndex = static_cast<int>(i);
      bestStartTick = event.startTick;
    }
  }

  return bestIndex;
}

}  // namespace RuntimePhraseEdit

#endif  // GROOVEPUTER_SRC_PHRASE_RUNTIME_PHRASE_EDIT_H
