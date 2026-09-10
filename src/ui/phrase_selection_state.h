#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_SELECTION_STATE_H
#define GROOVEPUTER_SRC_UI_PHRASE_SELECTION_STATE_H

#include <cstdint>

#include "src/phrase/runtime_phrase_edit.h"

// U4D1: the one selected event, shared by every view of a melody.
//
// The piano roll addresses a cursor tick, which cannot distinguish two events
// that start together -- and the data allows that. A list addresses the event
// itself. Both need the same answer to "which sound is selected", or switching
// views would quietly select something else.
//
// An array index is not that answer on its own: delete shifts the array, join
// removes an element, and undo replaces the whole buffer. So the selection also
// carries what the sound *is*, re-finds itself after any mutation, and falls to
// the nearest survivor when the sound it named is gone -- after an edit the user
// is somewhere, never nowhere.
//
// This is UI state and nothing more. It holds no music, it is not persisted
// across sessions, and every operation still goes through RuntimePhraseEdit.
namespace PhraseSelectionState {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct State {
  bool active = false;
  uint16_t eventIndex = 0;
  // What the selected sound is, so it can be recognised again after the array
  // underneath has moved.
  uint16_t startTick = 0;
  uint8_t note = 0;
  // Where a new sound would go. Deliberately independent: sharing it with the
  // selection is what made adding unreachable once LEFT/RIGHT began jumping
  // between onsets.
  uint16_t insertTick = 0;
};

inline State at(const Buffer& phrase, uint16_t index) {
  State out{};
  if (index >= phrase.count) return out;
  out.active = true;
  out.eventIndex = index;
  out.startTick = phrase.events[index].startTick;
  out.note = phrase.events[index].note;
  return out;
}

// Rows are ordered by start time, ties broken by buffer index so the order is
// stable and two coincident sounds keep distinct, repeatable places.
inline bool precedes(const Buffer& phrase, uint16_t a, uint16_t b) {
  const uint16_t startA = phrase.events[a].startTick;
  const uint16_t startB = phrase.events[b].startTick;
  if (startA != startB) return startA < startB;
  return a < b;
}

inline State first(const Buffer& phrase) {
  if (phrase.count == 0) return State{};
  uint16_t best = 0;
  for (uint16_t i = 1; i < phrase.count; ++i) {
    if (precedes(phrase, i, best)) best = i;
  }
  return at(phrase, best);
}

// Re-find the selected sound after the buffer changed under it.
inline State resolve(const Buffer& phrase, const State& state) {
  State out = state;
  if (phrase.count == 0) return State{};
  if (!state.active) return State{};

  // Unchanged slot: the common case, and the cheapest.
  if (state.eventIndex < phrase.count &&
      phrase.events[state.eventIndex].startTick == state.startTick &&
      phrase.events[state.eventIndex].note == state.note) {
    return out;
  }

  // The array moved: find the same sound wherever it is now.
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].startTick == state.startTick &&
        phrase.events[i].note == state.note) {
      out.eventIndex = i;
      return out;
    }
  }

  // The sound is gone -- deleted, or absorbed by a join. Land on the nearest
  // survivor in time rather than dropping the selection entirely.
  uint16_t best = 0;
  uint32_t bestDistance = UINT32_MAX;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    const uint16_t start = phrase.events[i].startTick;
    const uint32_t distance = start > state.startTick
        ? static_cast<uint32_t>(start - state.startTick)
        : static_cast<uint32_t>(state.startTick - start);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = i;
    }
  }
  State fallback = at(phrase, best);
  fallback.insertTick = state.insertTick;
  return fallback;
}

// Previous/next sound in row order. The ends hold rather than wrap: wrapping
// would turn the last press before the end of a melody into a jump home.
inline State step(const Buffer& phrase, const State& state, int direction) {
  if (direction != -1 && direction != 1) return state;
  const State current = resolve(phrase, state);
  if (!current.active) return first(phrase);

  int best = -1;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (i == current.eventIndex) continue;
    const bool ahead = direction > 0
        ? precedes(phrase, current.eventIndex, i)
        : precedes(phrase, i, current.eventIndex);
    if (!ahead) continue;
    if (best < 0) {
      best = static_cast<int>(i);
      continue;
    }
    const bool closer = direction > 0
        ? precedes(phrase, i, static_cast<uint16_t>(best))
        : precedes(phrase, static_cast<uint16_t>(best), i);
    if (closer) best = static_cast<int>(i);
  }
  if (best < 0) return current;

  State out = at(phrase, static_cast<uint16_t>(best));
  out.insertTick = current.insertTick;
  return out;
}

inline State withInsertTick(const State& state, uint16_t insertTick) {
  State out = state;
  out.insertTick = insertTick;
  return out;
}

}  // namespace PhraseSelectionState

#endif  // GROOVEPUTER_SRC_UI_PHRASE_SELECTION_STATE_H
