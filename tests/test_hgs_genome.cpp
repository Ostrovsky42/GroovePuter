#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>

#include "src/generation/generation_context.h"
#include "src/generation/harmony/song_variant_genome.h"
#include "src/generation/harmony/genre_harmonic_policy.h"
#include "tests/fixtures/harmony/song_variant_genome_golden.h"

using namespace GroovePuterHarmony;
using namespace GroovePuterRhythm;

namespace {
bool anyDifference(const SongVariantGenome& a, const SongVariantGenome& b) {
  const uint8_t* left = reinterpret_cast<const uint8_t*>(&a);
  const uint8_t* right = reinterpret_cast<const uint8_t*>(&b);
  for (uint8_t i = 0; i < sizeof(SongVariantGenome); ++i) {
    if (left[i] != right[i]) return true;
  }
  return false;
}

void assertWithinPolicyCaps(const SongVariantGenome& genome,
                            const GenreHarmonicPolicy& policy) {
  assert(genome.homeGravity <= policy.priors.home * 63u);
  assert(genome.harmonicMobility <= policy.priors.mobility * 63u);
  assert(genome.smoothnessBias <= policy.featureWeights.pcvlPenalty * 51u);
  assert(genome.commonToneBias <= policy.featureWeights.commonTone * 42u);
  assert(genome.functionalBias <= policy.priors.functional * 63u);
  assert(genome.modalBias <= policy.priors.modal * 63u);
  assert(genome.chromaticRisk <= policy.maxOutsideBreak * 63u);
  assert(genome.modulationRisk <= policy.maxModulationsBreak * 127u);
  assert(genome.portalRisk <= (policy.allowPortals ? policy.priors.augmented * 63u : 0u));
  assert(genome.extensionComplexity <= policy.qualityTier * 85u);
  assert(genome.verticalColor <= policy.colorBudget * 85u);
  assert(genome.corpusConservatism <= policy.priors.home * 63u);
  assert(genome.repetitionTolerance <= policy.priors.home * 63u);
  assert(genome.cadenceStrength <= policy.priors.functional * 63u);
}
}  // namespace

int main() {
  using namespace GroovePuterHarmonyTestFixtures;
  static_assert(sizeof(SongVariantGenome) == 14);
  static_assert(kGoldenModelVersion == 7);
  static_assert(sizeof(kCorpusModelSha256) == 65);
  static_assert(static_cast<uint8_t>(GenerationDomain::ArchetypeSelection) == 0);
  static_assert(static_cast<uint8_t>(GenerationDomain::RhythmIdentity) == 1);
  static_assert(static_cast<uint8_t>(GenerationDomain::P1Variation) == 2);
  static_assert(static_cast<uint8_t>(GenerationDomain::P2Variation) == 3);
  static_assert(static_cast<uint8_t>(GenerationDomain::P3Transformation) == 4);
  static_assert(static_cast<uint8_t>(GenerationDomain::TransformationIntent) == 5);
  static_assert(static_cast<uint8_t>(GenerationDomain::DrumsOrnament) == 6);
  static_assert(static_cast<uint8_t>(GenerationDomain::BassPitch) == 7);
  static_assert(static_cast<uint8_t>(GenerationDomain::ChordPitch) == 8);
  static_assert(static_cast<uint8_t>(GenerationDomain::LeadPitch) == 9);
  static_assert(static_cast<uint8_t>(GenerationDomain::FeelExpression) == 10);
  static_assert(static_cast<uint8_t>(GenerationDomain::PhysicalBinding) == 11);
  static_assert(static_cast<uint8_t>(GenerationDomain::BarEvolution) == 12);
  static_assert(static_cast<uint8_t>(GenerationDomain::BassRhythmSelection) == 13);
  static_assert(static_cast<uint8_t>(GenerationDomain::ChordRhythmSelection) == 14);
  static_assert(static_cast<uint8_t>(GenerationDomain::MelodicRhythmSelection) == 15);
  static_assert(static_cast<uint8_t>(GenerationDomain::MotifSelection) == 16);
  static_assert(static_cast<uint8_t>(GenerationDomain::FeelProfileSelection) == 17);
  static_assert(static_cast<uint8_t>(GenerationDomain::PhraseLawSelection) == 18);
  static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicGenome) == 19);
  static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicCandidateTieBreak) == 20);
  static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicRoute) == 21);
  static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicModulation) == 22);
  static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicVerticalColor) == 23);
  static_assert(static_cast<uint8_t>(GenerationDomain::PerformanceArp) == 24);
  static_assert(static_cast<uint8_t>(GenerationDomain::Count) == 25);

  for (uint8_t style = 0; style < 17; ++style) {
    const auto id = static_cast<HarmonicStyleId>(style);
    const GenreHarmonicPolicy* policy = policyFor(id);
    assert(policy != nullptr);
    for (uint32_t seed = 0; seed < 256; ++seed) {
      const SongVariantGenome genome = deriveSongVariantGenome(
          seed, 0x9876a543u, id, 1);
      assertWithinPolicyCaps(genome, *policy);
    }
  }

  const SongVariantGenome repeatedA =
      deriveSongVariantGenome(42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7);
  const SongVariantGenome repeatedB =
      deriveSongVariantGenome(42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7);
  assert(!anyDifference(repeatedA, repeatedB));
  assert(anyDifference(repeatedA, deriveSongVariantGenome(
      42, 0x1234abceu, HarmonicStyleId::UkGarage, 7)));
  assert(anyDifference(repeatedA, deriveSongVariantGenome(
      42, 0x1234abcdu, HarmonicStyleId::DnB, 7)));
  assert(anyDifference(repeatedA, deriveSongVariantGenome(
      42, 0x1234abcdu, HarmonicStyleId::UkGarage, 8)));

  for (const GenomeGoldenVector& vector : kGenomeGoldenVectors) {
    const SongVariantGenome actual = deriveSongVariantGenome(
        vector.projectSeed, kGoldenSongIdentity,
        static_cast<HarmonicStyleId>(kGoldenStyleId), kGoldenModelVersion);
    const uint8_t* actualBytes = reinterpret_cast<const uint8_t*>(&actual);
    for (uint8_t i = 0; i < vector.bytes.size(); ++i) {
      assert(actualBytes[i] == vector.bytes[i]);
    }
  }

  assert(deriveHarmonicDomainSeed(
             42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
             static_cast<GenerationDomain>(18)) == 0);
  assert(deriveHarmonicDomainSeed(
             42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
             static_cast<GenerationDomain>(25)) == 0);
  assert(!anyDifference(SongVariantGenome{}, deriveSongVariantGenome(
      42, 0x1234abcdu, static_cast<HarmonicStyleId>(255), 7)));

  // Genome generation and future route/color/tie-break decisions use separate
  // reserved domains, so evaluating one does not advance shared RNG state.
  const uint32_t routeBefore = deriveHarmonicDomainSeed(
      42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
      GenerationDomain::HarmonicRoute);
  (void)deriveSongVariantGenome(42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7);
  (void)deriveHarmonicDomainSeed(42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
                                 GenerationDomain::HarmonicVerticalColor);
  (void)deriveHarmonicDomainSeed(42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
                                 GenerationDomain::HarmonicCandidateTieBreak);
  const uint32_t routeAfter = deriveHarmonicDomainSeed(
      42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
      GenerationDomain::HarmonicRoute);
  assert(routeBefore == routeAfter);
  assert(routeBefore != deriveHarmonicDomainSeed(
      42, 0x1234abcdu, HarmonicStyleId::UkGarage, 7,
      GenerationDomain::HarmonicVerticalColor));

  std::set<uint8_t> homeValues;
  for (uint32_t seed = 0; seed < 256; ++seed) {
    homeValues.insert(deriveSongVariantGenome(
        seed, 0x1234abcdu, HarmonicStyleId::UkGarage, 7).homeGravity);
  }
  assert(homeValues.size() > 1);

  std::cout << "HGS song variant genome: PASS\n";
  return 0;
}
