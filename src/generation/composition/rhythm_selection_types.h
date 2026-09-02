#ifndef GROOVEPUTER_GENERATION_COMPOSITION_RHYTHM_SELECTION_TYPES_H
#define GROOVEPUTER_GENERATION_COMPOSITION_RHYTHM_SELECTION_TYPES_H

#include <cstdint>

namespace GroovePuterRhythm {

// Persisted user intent. AUTO stores no derived vocabulary identity; MANUAL
// stores one stable RhythmArchetypeId in GenreSettings.
enum class RhythmSelectionMode : uint8_t {
  Auto = 0,
  Manual,
  Count,
};

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_RHYTHM_SELECTION_TYPES_H
