#pragma once

#include <cstdint>

#include "rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

struct GenerationContext {
  uint32_t projectSeed = 0;
  uint16_t phraseOrdinal = 0;
};

// Stable, allocation-free seed derivation. RhythmIdentity deliberately does
// not depend on RealizationLevel so P1/P2/P3 can share one phrase identity.
uint32_t deriveGenerationSeed(const GenerationContext& context,
                              RhythmArchetypeId archetypeId,
                              GenerationDomain domain,
                              uint32_t salt = 0);

uint32_t deriveVariationSeed(uint32_t identitySeed,
                             RealizationLevel level,
                             uint32_t salt = 0);

uint32_t deterministicValue(uint32_t seed, uint32_t coordinate);

}  // namespace GroovePuterRhythm
