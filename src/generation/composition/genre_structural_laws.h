#ifndef GROOVEPUTER_GENERATION_COMPOSITION_GENRE_STRUCTURAL_LAWS_H
#define GROOVEPUTER_GENERATION_COMPOSITION_GENRE_STRUCTURAL_LAWS_H

#include <cstdint>

#include "generation_profile.h"
#include "../roles/harmonic_rhythm.h"

namespace GroovePuterRhythm {

// Harmonic change rate is a musician-facing structural decision: how many
// quarter-note beats normally pass before moving harmony advances. It is not a
// texture/effect parameter and it deliberately derives from the authoritative
// generation profile identity rather than from a parallel genre table.
//
// Base Lo-Fi and its slow/sparse recipes leave a full 4/4 bar for each harmonic
// state. Lo-Fi House is an explicit musical exception: its four-floor grammar
// keeps the established two-beat harmonic motion used by the faster profiles.
inline HarmonicChangeRateId harmonicChangeRateForProfile(
    const GenerationProfileView& profile) {
  if (profile.generativeMode != static_cast<uint8_t>(GenerativeMode::LoFi)) {
    return HarmonicChangeRateId::Every2Beats;
  }
  if (profile.recipe == kLoFiHouseRecipeId) {
    return HarmonicChangeRateId::Every2Beats;
  }
  return HarmonicChangeRateId::Every4Beats;
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_GENRE_STRUCTURAL_LAWS_H
