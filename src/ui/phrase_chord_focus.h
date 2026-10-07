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
  NoChord,
  TooShort,
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

// A: over a single note, the whole triad of the key at once (one key press
// gives a chord, no theory needed); over a chord, one more tone on top (a
// triad becomes a seventh chord). New tones start with `base` and last as
// long; `newIndex` is the highest one added. The caller edits pitch after.
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
  const bool single = sameStart <= 1;
  const int third = chordToneAbove(top, single, rootPitchClass, scale);
  const int fifth = single ? chordToneAbove(third, false, rootPitchClass, scale)
                           : third;
  if (fifth > 127) return AddResult::PitchLimit;
  if (single && live.count + 2u > PhraseRuntime::kMaxSynthEvents) {
    return AddResult::Full;
  }

  after = live;
  const int tones[2] = {third, fifth};
  for (uint8_t t = 0; t < (single ? 2 : 1); ++t) {
    auto& added = after.events[after.count];
    added = source;
    added.note = static_cast<uint8_t>(tones[t]);
    newIndex = after.count;
    ++after.count;
  }
  if (!RuntimePhraseEdit::validate(after)) {
    newIndex = -1;
    return AddResult::Invalid;
  }
  return AddResult::Ready;
}

// Alt+A: the chord in the cursor cell becomes an arpeggio. Its notes play one
// after another on the grid, low to high and around again, for as long as the
// chord lasted (cut at the next note that starts later). Nothing overlaps
// afterwards, so the internal synth plays it too. `steps` gets the note count.
inline AddResult prepareArpeggio(const Buffer& live, uint16_t cellTick,
                                 uint16_t cellTicks, uint16_t gridTicks,
                                 Buffer& after, uint16_t& steps) {
  steps = 0;
  uint16_t chord[kMaxCellNotes];
  const uint8_t n = notesInCell(live, cellTick, cellTicks, chord, kMaxCellNotes);
  if (n < 2) return AddResult::NoChord;
  if (gridTicks == 0) return AddResult::Invalid;

  uint32_t start = 0xFFFFFFFFu;
  uint32_t end = 0;
  for (uint8_t i = 0; i < n; ++i) {
    const auto& e = live.events[chord[i]];
    const uint32_t s = static_cast<uint32_t>(e.startTick) * PhraseRuntime::kSubticksPerTick;
    if (e.startTick < start) start = e.startTick;
    const uint32_t eEnd = (s + e.durationSubticks) / PhraseRuntime::kSubticksPerTick;
    if (eEnd > end) end = eEnd;
  }
  bool inChord[PhraseRuntime::kMaxSynthEvents] = {};
  for (uint8_t i = 0; i < n; ++i) inChord[chord[i]] = true;
  for (uint16_t i = 0; i < live.count; ++i) {
    if (inChord[i]) continue;
    const uint32_t s = live.events[i].startTick;
    if (s > start && s < end) end = s;
  }
  if (end > live.lengthTicks) end = live.lengthTicks;
  const uint32_t count = (end - start) / gridTicks;
  if (count < 2) return AddResult::TooShort;
  if (live.count - n + count > PhraseRuntime::kMaxSynthEvents) {
    return AddResult::Full;
  }

  after = live;
  after.count = 0;
  for (uint16_t i = 0; i < live.count; ++i) {
    if (!inChord[i]) after.events[after.count++] = live.events[i];
  }
  for (uint32_t k = 0; k < count; ++k) {
    auto& e = after.events[after.count++];
    e = live.events[chord[k % n]];
    e.startTick = static_cast<uint16_t>(start + k * gridTicks);
    e.durationSubticks = static_cast<decltype(e.durationSubticks)>(
        gridTicks * PhraseRuntime::kSubticksPerTick);
  }
  // Keep the buffer in time order.
  for (uint16_t i = 1; i < after.count; ++i) {
    const auto moving = after.events[i];
    uint16_t j = i;
    while (j > 0 && after.events[j - 1].startTick > moving.startTick) {
      after.events[j] = after.events[j - 1];
      --j;
    }
    after.events[j] = moving;
  }
  if (!RuntimePhraseEdit::validate(after)) return AddResult::Invalid;
  steps = static_cast<uint16_t>(count);
  return AddResult::Ready;
}

}  // namespace PhraseChordFocus
