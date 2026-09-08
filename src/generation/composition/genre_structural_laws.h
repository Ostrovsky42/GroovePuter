#ifndef GROOVEPUTER_GENERATION_COMPOSITION_GENRE_STRUCTURAL_LAWS_H
#define GROOVEPUTER_GENERATION_COMPOSITION_GENRE_STRUCTURAL_LAWS_H

#include <cstdint>

#include "generation_profile.h"
#include "../roles/harmonic_rhythm.h"

namespace GroovePuterRhythm {

// Harmonic change rate is a musician-facing structural decision: how many
// quarter-note beats normally pass before moving harmony advances. Profile data
// is the single genre/recipe owner; this accessor does not reinterpret identity.
inline HarmonicChangeRateId harmonicChangeRateForProfile(
    const GenerationProfileView& profile) {
  return profile.harmonicChangeRate;
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_GENRE_STRUCTURAL_LAWS_H
