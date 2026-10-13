#ifndef GROOVEPUTER_GENERATION_HARMONY_SONG_VARIANT_GENOME_H
#define GROOVEPUTER_GENERATION_HARMONY_SONG_VARIANT_GENOME_H

#include <cstdint>
#include <type_traits>

#include "genre_harmonic_policy.h"
#include "src/generation/rhythm/rhythm_types.h"

namespace GroovePuterHarmony {

struct SongVariantGenome {
  uint8_t homeGravity = 0;
  uint8_t harmonicMobility = 0;
  uint8_t smoothnessBias = 0;
  uint8_t commonToneBias = 0;
  uint8_t functionalBias = 0;
  uint8_t modalBias = 0;
  uint8_t chromaticRisk = 0;
  uint8_t modulationRisk = 0;
  uint8_t portalRisk = 0;
  uint8_t extensionComplexity = 0;
  uint8_t verticalColor = 0;
  uint8_t corpusConservatism = 0;
  uint8_t repetitionTolerance = 0;
  uint8_t cadenceStrength = 0;
};

static_assert(sizeof(SongVariantGenome) == 14,
              "song variant genome must remain exactly 14 bytes");
static_assert(std::is_trivially_copyable<SongVariantGenome>::value,
              "song variant genome must be a fixed value type");

uint32_t deriveHarmonicDomainSeed(
    uint32_t projectSeed, uint32_t songIdentity, HarmonicStyleId style,
    uint16_t modelVersion, GroovePuterRhythm::GenerationDomain domain,
    uint8_t fieldSalt = 0);

SongVariantGenome deriveSongVariantGenome(uint32_t projectSeed,
                                          uint32_t songIdentity,
                                          HarmonicStyleId style,
                                          uint16_t modelVersion);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_SONG_VARIANT_GENOME_H
