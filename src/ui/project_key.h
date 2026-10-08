#pragma once

#include <cstdint>
#include <cstdio>

#include "src/generation/tonal/scale_catalog.h"
#include "src/input/performance_instrument_types.h"

// The project key (0.9.17): tonic and scale from Scene::generatorParams, the
// same key G generates in. The Melody editor shows it, changes it (K / M) and
// moves notes along it (Up/Down), so a melody stays in tune without theory.
namespace ProjectKey {

using GroovePuterRhythm::ScaleTypeValue;

inline uint8_t pitchClass(int value) {
  return static_cast<uint8_t>((value % 12 + 12) % 12);
}

inline bool inScale(int note, uint8_t root, ScaleTypeValue scale) {
  const auto def = GroovePuterRhythm::scaleDefinitionFor(scale);
  if (def.intervals == nullptr) return true;
  const int degree = pitchClass(note - root);
  for (uint8_t i = 0; i < def.count; ++i) {
    if (def.intervals[i] == degree) return true;
  }
  return false;
}

// The next note of the key above (direction > 0) or below `note`; a note
// outside the key goes to the nearest key note that way. -1 past 0..127.
inline int step(int note, int direction, uint8_t root, ScaleTypeValue scale) {
  const int dir = direction > 0 ? 1 : -1;
  for (int candidate = note + dir; candidate >= 0 && candidate <= 127;
       candidate += dir) {
    if (inScale(candidate, root, scale)) return candidate;
  }
  return -1;
}

inline const char* scaleShortName(ScaleTypeValue scale) {
  switch (scale) {
    case GroovePuterRhythm::kScaleMinor: return "MIN";
    case GroovePuterRhythm::kScaleMajor: return "MAJ";
    case GroovePuterRhythm::kScaleDorian: return "DOR";
    case GroovePuterRhythm::kScalePhrygian: return "PHR";
    case GroovePuterRhythm::kScaleLydian: return "LYD";
    case GroovePuterRhythm::kScaleMixolydian: return "MIX";
    case GroovePuterRhythm::kScaleLocrian: return "LOC";
    case GroovePuterRhythm::kScalePentatonicMajor: return "PMAJ";
    case GroovePuterRhythm::kScalePentatonicMinor: return "PMIN";
    case GroovePuterRhythm::kScaleChromatic: return "CHR";
    default: return "?";
  }
}

inline const char* rootName(uint8_t root) {
  static constexpr const char* kNames[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                             "F#", "G",  "G#", "A",  "A#", "B"};
  return kNames[root % 12];
}

// "KEY C DOR"
inline void format(uint8_t root, ScaleTypeValue scale, char* out,
                   std::size_t size) {
  std::snprintf(out, size, "KEY %s %s", rootName(root), scaleShortName(scale));
}

inline ScaleTypeValue nextScale(ScaleTypeValue scale) {
  return static_cast<ScaleTypeValue>((scale + 1u) %
                                     GroovePuterRhythm::kScaleTypeCount);
}

// KEYBOARD plays in the project key too (0.9.17): same ten scales, other order.
inline PerformanceScale toPerformanceScale(ScaleTypeValue scale) {
  using namespace GroovePuterRhythm;
  switch (scale) {
    case kScaleMinor: return PerformanceScale::NaturalMinor;
    case kScaleMajor: return PerformanceScale::Major;
    case kScaleDorian: return PerformanceScale::Dorian;
    case kScalePhrygian: return PerformanceScale::Phrygian;
    case kScaleLydian: return PerformanceScale::Lydian;
    case kScaleMixolydian: return PerformanceScale::Mixolydian;
    case kScaleLocrian: return PerformanceScale::Locrian;
    case kScalePentatonicMajor: return PerformanceScale::MajorPentatonic;
    case kScalePentatonicMinor: return PerformanceScale::MinorPentatonic;
    default: return PerformanceScale::Chromatic;
  }
}

inline ScaleTypeValue fromPerformanceScale(PerformanceScale scale) {
  using namespace GroovePuterRhythm;
  switch (scale) {
    case PerformanceScale::NaturalMinor: return kScaleMinor;
    case PerformanceScale::Major: return kScaleMajor;
    case PerformanceScale::Dorian: return kScaleDorian;
    case PerformanceScale::Phrygian: return kScalePhrygian;
    case PerformanceScale::Lydian: return kScaleLydian;
    case PerformanceScale::Mixolydian: return kScaleMixolydian;
    case PerformanceScale::Locrian: return kScaleLocrian;
    case PerformanceScale::MajorPentatonic: return kScalePentatonicMajor;
    case PerformanceScale::MinorPentatonic: return kScalePentatonicMinor;
    default: return kScaleChromatic;
  }
}

}  // namespace ProjectKey
