#include "generation_context.h"

namespace GroovePuterRhythm {
namespace {

uint32_t mix32(uint32_t value) {
  value += 0x9E3779B9u;
  value = (value ^ (value >> 16u)) * 0x85EBCA6Bu;
  value = (value ^ (value >> 13u)) * 0xC2B2AE35u;
  return value ^ (value >> 16u);
}

GenerationDomain variationDomain(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical:
      return GenerationDomain::P1Variation;
    case RealizationLevel::P2Variation:
      return GenerationDomain::P2Variation;
    case RealizationLevel::P3Transformation:
      return GenerationDomain::P3Transformation;
    default:
      return GenerationDomain::P1Variation;
  }
}

}  // namespace

uint32_t deriveGenerationSeed(const GenerationContext& context,
                              RhythmArchetypeId archetypeId,
                              GenerationDomain domain,
                              uint32_t salt) {
  uint32_t value = mix32(context.projectSeed ^ 0x47525631u);
  value = mix32(value ^ (static_cast<uint32_t>(archetypeId) << 16u));
  value = mix32(value ^ static_cast<uint32_t>(context.phraseOrdinal));
  value = mix32(value ^ (static_cast<uint32_t>(domain) * 0x45D9F3Bu));
  return mix32(value ^ salt);
}

uint32_t deriveVariationSeed(uint32_t identitySeed,
                             RealizationLevel level,
                             uint32_t salt) {
  const GenerationDomain domain = variationDomain(level);
  uint32_t value = mix32(identitySeed ^ 0x56415231u);
  value = mix32(value ^ (static_cast<uint32_t>(domain) * 0x27D4EB2Du));
  return mix32(value ^ salt);
}

uint32_t deterministicValue(uint32_t seed, uint32_t coordinate) {
  return mix32(seed ^ mix32(coordinate + 0xA5A5A5A5u));
}

}  // namespace GroovePuterRhythm
