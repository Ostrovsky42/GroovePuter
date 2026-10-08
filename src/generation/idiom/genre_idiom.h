#pragma once

#include <cstdint>
#include <cstdlib>

#include "../../../scenes.h"
#include "../tonal/scale_catalog.h"
#include "genre_idiom_tables.h"

// 0.9.18 genre idioms (prototype: Acid, LoFi, HipHop, UK garage, House).
//
// Bass (Synth A) and lead (Synth B) come from one template, so they interlock
// by construction. Synth B plays the chord stabs as one moving top voice (the
// internal synth is mono) with the template's melody notes as its peaks.
// Over a phrase the bars follow A A' A B: A' keeps the rhythm and moves the
// voice to another chord tone, B changes the ending and lands on the tonic.
// Templates are in C minor and are mapped degree by degree onto the project
// key and scale. Research: docs/superpowers/plans/2026-10-08-0918-genre-idioms-research.md
namespace GenreIdiom {

constexpr uint8_t kNoBarOrdinal = 0xFF;

struct Request {
  uint8_t generativeMode = 0;
  uint8_t recipe = 0;
  uint8_t rootPitchClass = 0;
  uint8_t scale = GroovePuterRhythm::kScaleMinor;
  uint8_t level = 1;  // template level 0..2 (P1, P2, P3)
  uint8_t barOrdinal = kNoBarOrdinal;
  uint32_t salt = 0;
};

namespace detail {

inline int floorDiv12(int value) {
  return value >= 0 ? value / 12 : -((11 - value) / 12);
}

// A C-minor semitone onto the project scale: same degree, same octave; a
// chromatic note keeps its offset from the degree below it.
inline int mapSemi(int semi, uint8_t scale) {
  static constexpr int8_t kMinorDegree[12] = {0, -1, 1, 2, -1, 3, -1, 4, 5, -1, 6, -1};
  const auto definition = GroovePuterRhythm::scaleDefinitionFor(scale);
  if (definition.intervals == nullptr || definition.count >= 12) return semi;
  const int octave = floorDiv12(semi);
  int pc = semi - octave * 12;
  int chroma = 0;
  while (kMinorDegree[pc] < 0) {
    --pc;
    ++chroma;
  }
  const int degree = kMinorDegree[pc];
  int mapped = 0;
  if (definition.count == 7) {
    mapped = definition.intervals[degree];
  } else {
    // Pentatonic: the scale tone nearest to the minor degree.
    static constexpr int8_t kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    int best = 99;
    for (uint8_t i = 0; i < definition.count; ++i) {
      const int distance = std::abs(definition.intervals[i] - kMinor[degree]);
      if (distance < best) {
        best = distance;
        mapped = definition.intervals[i];
      }
    }
  }
  return octave * 12 + mapped + chroma;
}

inline void clear(SynthPattern& pattern) {
  for (auto& step : pattern.steps) step = SynthStep{};
}

inline void place(SynthPattern& pattern, uint8_t step, int pitch, uint8_t len,
                  uint8_t velocity, bool accent) {
  if (step >= SynthPattern::kSteps || pitch < 0 || pitch > 127) return;
  SynthStep note{};
  note.note = static_cast<int8_t>(pitch);
  note.velocity = velocity;
  note.accent = accent;
  pattern.steps[step] = note;
  for (uint8_t k = 1; k < len && step + k < SynthPattern::kSteps; ++k) {
    if (pattern.steps[step + k].note >= 0) break;
    SynthStep tie = note;
    tie.accent = false;
    tie.slide = true;  // a held note: same pitch, slid into
    pattern.steps[step + k] = tie;
  }
}

// The dataset marks a slide on the note it leaves (303); here the slide flag
// sits on the note it reaches, and the gap between them is held.
inline void connectSlides(SynthPattern& pattern, const uint8_t (&slideOut)[16]) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (!slideOut[step] || pattern.steps[step].note < 0) continue;
    int next = step + 1;
    while (next < SynthPattern::kSteps &&
           (pattern.steps[next].note < 0 ||
            (pattern.steps[next].slide && pattern.steps[next].note == pattern.steps[step].note))) {
      ++next;
    }
    if (next >= SynthPattern::kSteps) break;
    for (int gap = step + 1; gap < next; ++gap) {
      SynthStep tie = pattern.steps[step];
      tie.accent = false;
      tie.slide = true;
      pattern.steps[gap] = tie;
    }
    pattern.steps[next].slide = true;
  }
}

}  // namespace detail

// The template for a genre/recipe, or nullptr where the genre keeps the
// regular generator.
inline const IdiomVariant* variantFor(uint8_t generativeMode, uint8_t recipe,
                                      uint32_t salt) {
  switch (generativeMode) {
    case 0:  // Acid
      if (recipe != 0) return nullptr;
      return (salt & 1u) ? &k_acid_rolling : &k_acid_chicago_jack;
    case 9:  // House
      return recipe == 0 ? &k_house_deep_offbeat : nullptr;
    case 11:  // HipHop
      if (recipe == 0 || recipe == 16) return &k_boombap_golden_era;
      if (recipe == 17) return &k_boombap_dusty_jazz;
      return nullptr;
    case 13:  // UK garage
      if (recipe != 0) return nullptr;
      return (salt & 1u) ? &k_ukg_dark_skippy : &k_ukg_classic_2step;
    case 15:  // LoFi
      if (recipe == 0 || recipe == 12) return &k_lofi_classic_chill;
      if (recipe == 13) return &k_lofi_drunken;
      return nullptr;
    default:
      return nullptr;
  }
}

// Writes Synth A (bass) and Synth B (lead) for one bar. False (and nothing
// written) when the genre has no idiom.
inline bool apply(const Request& request, SynthPattern& bass, SynthPattern& lead) {
  const IdiomVariant* variant =
      variantFor(request.generativeMode, request.recipe, request.salt);
  if (variant == nullptr) return false;
  const IdiomLevel& level = variant->levels[request.level > 2 ? 2 : request.level];
  if (level.bass == nullptr) return false;

  const int role = request.barOrdinal == kNoBarOrdinal ? 0 : request.barOrdinal % 4;
  const bool answerBar = role == 3;
  const int root = request.rootPitchClass % 12;
  const int shift = root >= 6 ? root - 12 : root;  // keep registers near C
  const int bassBase = 36 + shift;
  const int leadBase = 60 + shift;
  const uint32_t salt = request.salt;
  auto map = [&](int semi) { return detail::mapSemi(semi, request.scale); };

  // ---- Synth A: the template bass, one octave flip per press, a fill on B.
  detail::clear(bass);
  uint8_t slideOut[16] = {};
  const int flipIndex = level.bassCount > 1 && ((salt >> 2) & 1u)
      ? 1 + static_cast<int>((salt >> 3) % (level.bassCount - 1))
      : -1;
  for (uint8_t i = 0; i < level.bassCount; ++i) {
    const IdiomNote& note = level.bass[i];
    if (answerBar && note.step >= 12) continue;
    int semi = note.semi;
    if (static_cast<int>(i) == flipIndex) semi += semi < 6 ? 12 : -12;
    detail::place(bass, note.step, bassBase + map(semi), note.len, note.velocity,
                  (note.flags & kAccent) != 0);
    if (note.flags & kSlideOut) slideOut[note.step] = 1;
  }
  if (answerBar) {
    // Back to the root of the next bar: octave run with a glide, or a b7 pickup.
    if (level.bassCount >= 8) {
      detail::place(bass, 12, bassBase + map(0), 1, 104, true);
      detail::place(bass, 13, bassBase + map(12), 1, 84, false);
      detail::place(bass, 14, bassBase + map(0), 1, 92, false);
      detail::place(bass, 15, bassBase + map(12), 1, 96, false);
      slideOut[14] = 1;
    } else {
      detail::place(bass, 14, bassBase + map(-2), 2, 88, false);
    }
  }
  detail::connectSlides(bass, slideOut);

  // ---- Synth B: stabs as one moving top voice, melody notes as its peaks.
  detail::clear(lead);
  static constexpr int kTargets[4] = {7, 10, 3, 12};
  int previous = leadBase + kTargets[salt % 4];
  const int lo = leadBase - 3;
  const int hi = leadBase + 19;
  // A' (role 1) keeps the rhythm and lifts the line: every stab one chord tone
  // higher, every melody note one scale step higher.
  const int rankShift = role == 1 ? 1 : 0;
  bool inScale[12] = {};
  {
    static constexpr int8_t kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    for (int degree : kMinor) inScale[((map(degree) % 12) + 12) % 12] = true;
  }
  auto scaleStepUp = [&](int pitch) {
    for (int next = pitch + 1; next <= pitch + 4; ++next) {
      if (inScale[((next - leadBase) % 12 + 12) % 12]) return next;
    }
    return pitch;
  };
  int8_t melodyAt[16];
  for (auto& m : melodyAt) m = -1;
  for (uint8_t i = 0; i < level.melodyCount; ++i) melodyAt[level.melody[i].step] = static_cast<int8_t>(i);
  int lastStep = -1;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (answerBar && step >= 12) break;
    if (melodyAt[step] >= 0) {
      const IdiomNote& note = level.melody[melodyAt[step]];
      int pitch = leadBase + map(note.semi);
      if (rankShift > 0) pitch = scaleStepUp(pitch);
      detail::place(lead, step, pitch, note.len, note.velocity, (note.flags & kAccent) != 0);
      previous = pitch;
      lastStep = step;
      continue;
    }
    for (uint8_t i = 0; i < level.stabCount; ++i) {
      const IdiomStab& stab = level.stabs[i];
      if (stab.step != step) continue;
      bool tone[12] = {};
      for (int interval = 0; interval < 24; ++interval) {
        if ((stab.intervalMask & (1u << interval)) == 0) continue;
        tone[((map(stab.root + interval) % 12) + 12) % 12] = true;
      }
      int candidates[24];
      int count = 0;
      for (int pitch = lo; pitch <= hi && count < 24; ++pitch) {
        if (tone[((pitch - leadBase) % 12 + 12) % 12]) candidates[count++] = pitch;
      }
      // Move to the nearest chord tone, but not onto the same note.
      int bestIndex = -1;
      int bestCost = 1 << 30;
      for (int c = 0; c < count; ++c) {
        const int cost = std::abs(candidates[c] - previous) + (candidates[c] == previous ? 3 : 0);
        if (cost < bestCost) {
          bestCost = cost;
          bestIndex = c;
        }
      }
      int best = -1;
      if (bestIndex >= 0) {
        int index = bestIndex + rankShift;
        if (index >= count) index = count - 1;
        best = candidates[index];
      }
      if (best >= 0) {
        detail::place(lead, step, best, stab.len, stab.velocity, false);
        previous = best;
        lastStep = step;
      }
    }
  }
  if (answerBar) {
    // The answer lands on the tonic nearest the line and rings to the bar end.
    int tonic = leadBase;
    while (tonic + 12 <= previous + 6) tonic += 12;
    detail::place(lead, lastStep >= 11 ? 13 : 12, tonic, 4, 92, true);
  }
  return true;
}

}  // namespace GenreIdiom
