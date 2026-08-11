#ifndef GROOVEPUTER_GENERATION_COMPOSITION_TONAL_PROFILE_H
#define GROOVEPUTER_GENERATION_COMPOSITION_TONAL_PROFILE_H

#include <cstdint>
#include <type_traits>

#include "../roles/bass_pitch_behavior.h"
#include "../roles/melodic_pitch_intent.h"

struct GenreSettings;

namespace GroovePuterRhythm {

struct TonalRegisterCorridor {
  uint8_t minMidi = 24;
  uint8_t maxMidi = 71;
  uint8_t maxAdjacentLeapSemitones = 12;
};

struct TonalGenerationProfile {
  BassBehaviorPolicy bassPolicy{};
  MelodicIntentPolicy melodicPolicy{};
  TonalRegisterCorridor bassRegister{24, 47, 12};
  TonalRegisterCorridor secondaryRegister{48, 71, 16};
};

TonalGenerationProfile tonalGenerationProfileFor(const GenreSettings& settings);

static_assert(std::is_trivially_copyable<TonalRegisterCorridor>::value,
              "TonalRegisterCorridor must remain transient");
static_assert(std::is_trivially_copyable<TonalGenerationProfile>::value,
              "TonalGenerationProfile must remain transient");
static_assert(sizeof(TonalGenerationProfile) <= 32,
              "TonalGenerationProfile exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_TONAL_PROFILE_H
