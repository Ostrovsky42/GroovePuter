#pragma once

#include <cstdint>

#include "src/generation/tonal/scale_catalog.h"
#include "src/phrase/runtime_phrase_edit.h"

// Melody editor chords (0.9.17): which note of a chord the editor works on,
// and adding a chord tone. A "chord" is two or more notes starting in the
// cursor cell. Pure functions over the Melody buffer; the page owns state.
namespace PhraseChordFocus {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

// Indices of the notes starting in [cellTick, cellTick + cellTicks), sorted
// by pitch, low to high. Returns how many were written (at most `capacity`).
inline uint8_t notesInCell(const Buffer& phrase, uint16_t cellTick,
                           uint16_t cellTicks, uint16_t* out,
                           uint8_t capacity) {
  uint8_t n = 0;
  const uint32_t end = static_cast<uint32_t>(cellTick) + cellTicks;
  for (uint16_t i = 0; i < phrase.count && n < capacity; ++i) {
    const uint16_t start = phrase.events[i].startTick;
    if (start < cellTick || start >= end) continue;
    uint8_t at = n++;
    while (at > 0 &&
           phrase.events[out[at - 1]].note > phrase.events[i].note) {
      out[at] = out[at - 1];
      --at;
    }
    out[at] = i;
  }
  return n;
}

constexpr uint8_t kMaxCellNotes = 16;

// The focused note stays meaningful only while it is one of two or more
// notes starting in the cursor cell.
inline bool valid(const Buffer& phrase, uint16_t cellTick, uint16_t cellTicks,
                  int focus) {
  if (focus < 0 || focus >= phrase.count) return false;
  uint16_t notes[kMaxCellNotes];
  const uint8_t n = notesInCell(phrase, cellTick, cellTicks, notes,
                                kMaxCellNotes);
  if (n < 2) return false;
  for (uint8_t i = 0; i < n; ++i) {
    if (notes[i] == focus) return true;
  }
  return false;
}

// C: the next higher note of the chord (wrapping), or the lowest when nothing
// is focused. -1 when the cell holds fewer than two notes.
inline int next(const Buffer& phrase, uint16_t cellTick, uint16_t cellTicks,
                int focus, uint8_t* position = nullptr,
                uint8_t* total = nullptr) {
  uint16_t notes[kMaxCellNotes];
  const uint8_t n = notesInCell(phrase, cellTick, cellTicks, notes,
                                kMaxCellNotes);
  if (total) *total = n;
  if (n < 2) return -1;
  uint8_t at = 0;
  for (uint8_t i = 0; i < n; ++i) {
    if (notes[i] == focus) {
      at = static_cast<uint8_t>((i + 1u) % n);
      break;
    }
  }
  if (position) *position = at;
  return notes[at];
}

enum class AddResult : uint8_t {
  Ready,
  NoTarget,
  Full,
  PitchLimit,
  Invalid,
  AlreadyInChord,
};

// Keys landing within this many milliseconds of each other were played as one
// chord (an external keyboard sends a chord as consecutive NoteOns).
constexpr uint32_t kChordWindowMs = 40;

inline bool sameChordOnset(uint32_t previousMs, uint32_t nowMs) {
  return static_cast<uint32_t>(nowMs - previousMs) <= kChordWindowMs;
}

// Recording: `note` joins the chord of `base` (same start and length).
inline AddResult prepareAddNote(const Buffer& live, int base, uint8_t note,
                                uint8_t velocity, Buffer& after,
                                int& newIndex) {
  newIndex = -1;
  if (base < 0 || base >= live.count) return AddResult::NoTarget;
  if (live.count >= PhraseRuntime::kMaxSynthEvents) return AddResult::Full;
  if (note > 127) return AddResult::PitchLimit;
  const auto& source = live.events[base];
  for (uint16_t i = 0; i < live.count; ++i) {
    if (live.events[i].startTick == source.startTick &&
        live.events[i].note == note) {
      return AddResult::AlreadyInChord;
    }
  }
  after = live;
  auto& added = after.events[after.count];
  added = source;
  added.note = note;
  if (velocity >= 1 && velocity <= 127) added.velocity = velocity;
  newIndex = after.count;
  ++after.count;
  if (!RuntimePhraseEdit::validate(after)) {
    newIndex = -1;
    return AddResult::Invalid;
  }
  return AddResult::Ready;
}

// The chord tone above `top` in the project key: the lowest scale tone at
// least a minor third up. In a seven-note scale that is the diatonic third
// (major or minor as the key decides); in a pentatonic, the next tone that
// still sounds as a chord tone. Chromatic (or no scale) keeps the plain rule:
// a major third over a single note, a minor third over a chord.
inline int chordToneAbove(int top, bool single, uint8_t rootPitchClass,
                          GroovePuterRhythm::ScaleTypeValue scale) {
  const auto def = GroovePuterRhythm::scaleDefinitionFor(scale);
  if (def.intervals == nullptr || def.count >= 12) return top + (single ? 4 : 3);
  for (int note = top + 3; note <= top + 12; ++note) {
    const int degree = ((note - rootPitchClass) % 12 + 12) % 12;
    for (uint8_t i = 0; i < def.count; ++i) {
      if (def.intervals[i] == degree) return note;
    }
  }
  return top + (single ? 4 : 3);
}

// A: a chord tone above the chord's top note (see chordToneAbove), starting
// with `base` and lasting as long. The caller edits its pitch afterwards.
inline AddResult prepareAddTone(
    const Buffer& live, int base, Buffer& after, int& newIndex,
    uint8_t rootPitchClass = 0,
    GroovePuterRhythm::ScaleTypeValue scale = GroovePuterRhythm::kScaleChromatic) {
  newIndex = -1;
  if (base < 0 || base >= live.count) return AddResult::NoTarget;
  if (live.count >= PhraseRuntime::kMaxSynthEvents) return AddResult::Full;
  const auto& source = live.events[base];
  int top = source.note;
  uint8_t sameStart = 0;
  for (uint16_t i = 0; i < live.count; ++i) {
    if (live.events[i].startTick != source.startTick) continue;
    ++sameStart;
    if (live.events[i].note > top) top = live.events[i].note;
  }
  const int note =
      chordToneAbove(top, sameStart <= 1, rootPitchClass, scale);
  if (note > 127) return AddResult::PitchLimit;

  after = live;
  auto& added = after.events[after.count];
  added = source;
  added.note = static_cast<uint8_t>(note);
  newIndex = after.count;
  ++after.count;
  if (!RuntimePhraseEdit::validate(after)) {
    newIndex = -1;
    return AddResult::Invalid;
  }
  return AddResult::Ready;
}

}  // namespace PhraseChordFocus
