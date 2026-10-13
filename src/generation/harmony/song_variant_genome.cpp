#include "song_variant_genome.h"

#include "src/generation/generation_context.h"

namespace GroovePuterHarmony {
namespace {

uint8_t min8(uint8_t a, uint8_t b) { return a < b ? a : b; }

uint8_t sampleBias(uint8_t cap, uint32_t seed) {
  if (cap == 0) return 0;
  const int16_t spread = static_cast<int16_t>((cap + 7u) / 8u);
  const uint8_t randomByte = static_cast<uint8_t>(seed >> 24u);
  const uint16_t width = static_cast<uint16_t>(spread * 2 + 1);
  const int16_t jitter = static_cast<int16_t>(randomByte % width) - spread;
  const int16_t center = static_cast<int16_t>((2u * cap + 1u) / 3u);
  const int16_t value = static_cast<int16_t>(center + jitter);
  if (value < 0) return 0;
  return static_cast<uint8_t>(value > cap ? cap : value);
}

}  // namespace

uint32_t deriveHarmonicDomainSeed(
    uint32_t projectSeed, uint32_t songIdentity, HarmonicStyleId style,
    uint16_t modelVersion, GroovePuterRhythm::GenerationDomain domain,
    uint8_t fieldSalt) {
  const uint8_t styleId = static_cast<uint8_t>(style);
  const uint8_t domainId = static_cast<uint8_t>(domain);
  if (styleId >= static_cast<uint8_t>(HarmonicStyleId::Count) ||
      domainId < static_cast<uint8_t>(
                     GroovePuterRhythm::GenerationDomain::HarmonicGenome) ||
      domainId > static_cast<uint8_t>(
                     GroovePuterRhythm::GenerationDomain::PerformanceArp)) {
    return 0;
  }

  // Split the full 32-bit stable song identity across existing seed coordinates.
  const GroovePuterRhythm::GenerationContext context{
      projectSeed, static_cast<uint16_t>(songIdentity)};
  const GroovePuterRhythm::RhythmArchetypeId identityHigh =
      static_cast<GroovePuterRhythm::RhythmArchetypeId>(songIdentity >> 16u);
  const uint32_t salt = (static_cast<uint32_t>(modelVersion) << 16u) |
                        (static_cast<uint32_t>(styleId) << 8u) | fieldSalt;
  return GroovePuterRhythm::deriveGenerationSeed(context, identityHigh, domain,
                                                  salt);
}

SongVariantGenome deriveSongVariantGenome(uint32_t projectSeed,
                                          uint32_t songIdentity,
                                          HarmonicStyleId style,
                                          uint16_t modelVersion) {
  SongVariantGenome result{};
  const GenreHarmonicPolicy* policy = policyFor(style);
  if (policy == nullptr) return result;

  const uint8_t mobilityCap =
      static_cast<uint8_t>(policy->priors.mobility * 63u);
  const uint8_t homeCap = static_cast<uint8_t>(policy->priors.home * 63u);
  const uint8_t functionalCap =
      static_cast<uint8_t>(policy->priors.functional * 63u);
  const uint8_t modalCap = static_cast<uint8_t>(policy->priors.modal * 63u);
  const uint8_t chromaticCap = min8(
      static_cast<uint8_t>(policy->priors.chromatic * 63u),
      static_cast<uint8_t>(policy->maxOutsideBreak * 63u));
  const uint8_t modulationCap = min8(
      mobilityCap, static_cast<uint8_t>(policy->maxModulationsBreak * 127u));
  const uint8_t portalCap = policy->allowPortals
                                ? static_cast<uint8_t>(policy->priors.augmented * 63u)
                                : 0;
  const uint8_t complexityCap = static_cast<uint8_t>(policy->qualityTier * 85u);
  const uint8_t colorCap = min8(
      static_cast<uint8_t>(policy->priors.polychord * 63u),
      static_cast<uint8_t>(policy->colorBudget * 85u));
  const auto fieldValue = [&](uint8_t field, uint8_t cap) {
    return sampleBias(
        cap, deriveHarmonicDomainSeed(
                 projectSeed, songIdentity, style, modelVersion,
                 GroovePuterRhythm::GenerationDomain::HarmonicGenome, field));
  };
  result.homeGravity = fieldValue(0, homeCap);
  result.harmonicMobility = fieldValue(1, mobilityCap);
  result.smoothnessBias = fieldValue(
      2, static_cast<uint8_t>(policy->featureWeights.pcvlPenalty * 51u));
  result.commonToneBias = fieldValue(
      3, static_cast<uint8_t>(policy->featureWeights.commonTone * 42u));
  result.functionalBias = fieldValue(4, functionalCap);
  result.modalBias = fieldValue(5, modalCap);
  result.chromaticRisk = fieldValue(6, chromaticCap);
  result.modulationRisk = fieldValue(7, modulationCap);
  result.portalRisk = fieldValue(8, portalCap);
  result.extensionComplexity = fieldValue(9, complexityCap);
  result.verticalColor = fieldValue(10, colorCap);
  result.corpusConservatism = fieldValue(11, homeCap);
  result.repetitionTolerance = fieldValue(12, homeCap);
  result.cadenceStrength = fieldValue(13, functionalCap);
  return result;
}

}  // namespace GroovePuterHarmony
