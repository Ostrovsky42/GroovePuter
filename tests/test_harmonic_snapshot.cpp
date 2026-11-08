#include <cassert>
#include <cstdint>
#include <type_traits>

#if __has_include("src/generation/harmony/harmonic_snapshot.h")
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/harmony/harmonic_snapshot.h"
#include "src/generation/roles/chord_progression.h"
#include "src/generation/tonal/scale_catalog.h"
#include "src/generation/rhythm/rhythm_types.h"

using namespace GroovePuterHarmony;

static HarmonySnapshot validSnapshot() {
  HarmonySnapshot snapshot{};
  snapshot.key.materialId = 1;
  snapshot.key.publicationGeneration = 1;
  snapshot.key.transportEpoch = 1;
  snapshot.key.eventOrdinal = 300;
  snapshot.durationSteps = 4;
  snapshot.base.qualityId = 0;
  snapshot.base.rootPc = 0;
  snapshot.base.bassPc = 255;
  snapshot.base.pitchMask = 0x091u; // C major: C, E, G.
  snapshot.tonal.scaleId = 1;
  snapshot.tonal.homeScaleId = 1;
  snapshot.tonal.scaleMask = 0x0ab5u;
  snapshot.tonal.homeScaleMask = 0x0ab5u;
  snapshot.tonal.tonicPc = 0;
  snapshot.tonal.homeTonicPc = 0;
  snapshot.status = HarmonyStatus::Ready;
  return snapshot;
}

int main() {
  static_assert(std::is_trivially_copyable<HarmonySnapshot>::value,
                "snapshot must remain a bounded value type");
  static_assert(static_cast<uint8_t>(GroovePuterRhythm::ChordQuality::Triad) == 0,
                "legacy quality IDs changed");
  static_assert(static_cast<uint8_t>(GroovePuterRhythm::ChordQuality::Diminished) == 7,
                "legacy quality IDs changed");
  static_assert(GroovePuterRhythm::kScaleMinor == 0 &&
                    GroovePuterRhythm::kScaleChromatic == 9,
                "legacy scale ABI changed");
  static_assert(static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::ArchetypeSelection) == 0 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::RhythmIdentity) == 1 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::P1Variation) == 2 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::P2Variation) == 3 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::P3Transformation) == 4 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::TransformationIntent) == 5 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::DrumsOrnament) == 6 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::BassPitch) == 7 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::ChordPitch) == 8 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::LeadPitch) == 9 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::FeelExpression) == 10 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::PhysicalBinding) == 11 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::BarEvolution) == 12 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::BassRhythmSelection) == 13 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::ChordRhythmSelection) == 14 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::MelodicRhythmSelection) == 15 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::MotifSelection) == 16 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::FeelProfileSelection) == 17 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::PhraseLawSelection) == 18,
                "legacy generation domains changed");
  static_assert(static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::HarmonicGenome) == 19 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::PerformanceArp) == 24 &&
                    static_cast<uint8_t>(GroovePuterRhythm::GenerationDomain::Count) == 25,
                "new generation domains must append without renumbering old domains");

  const ChordQualityDefinition* major = qualityDefinition(0);
  const ChordQualityDefinition* minor = qualityDefinition(1);
  assert(major != nullptr && major->intervalMask == 0x091u);
  assert(minor != nullptr && minor->intervalMask == 0x089u);
  const PitchSet12 expectedQualityMasks[] = {
      0x091u, 0x089u, 0x491u, 0x049u, 0x111u,
      0x0a1u, 0x489u, 0x891u, 0x48du, 0x895u};
  const uint8_t expectedQualityCardinality[] = {3, 3, 4, 3, 3, 3, 4, 4, 5, 5};
  for (QualityId id = 0; id < 10; ++id) {
    const auto* definition = qualityDefinition(id);
    assert(definition != nullptr && definition->intervalMask == expectedQualityMasks[id]);
    assert(definition->cardinality == expectedQualityCardinality[id]);
  }
  assert(qualityDefinition(10) == nullptr);

  const QualityId expectedLegacyQualityIds[] = {0, 6, 7, 2, 5, 8, 9, 3};
  for (uint8_t oldQuality = 0; oldQuality < 8; ++oldQuality) {
    const auto legacy = static_cast<GroovePuterRhythm::ChordQuality>(oldQuality);
    QualityId quality = 65535;
    assert(legacyQualityToId(legacy, quality));
    assert(quality == expectedLegacyQualityIds[oldQuality]);
    uint8_t legacyIntervals[5]{};
    uint8_t legacyCount = 0;
    assert(GroovePuterRhythm::chordQualityIntervals(
        legacy, legacyIntervals, 5, legacyCount));
    PitchSet12 legacyMask = 0;
    for (uint8_t i = 0; i < legacyCount; ++i)
      legacyMask = static_cast<PitchSet12>(legacyMask | (1u << (legacyIntervals[i] % 12u)));
    assert(legacyMask == qualityDefinition(quality)->intervalMask);
  }

  for (uint8_t oldScale = 0; oldScale < GroovePuterRhythm::kScaleTypeCount; ++oldScale) {
    ScaleId id = 65535;
    assert(legacyScaleToId(oldScale, id));
    const auto* definition = scaleDefinition(id);
    assert(definition != nullptr);
    const auto legacy = GroovePuterRhythm::scaleDefinitionFor(oldScale);
    PitchSet12 legacyMask = 0;
    for (uint8_t i = 0; i < legacy.count; ++i)
      legacyMask = static_cast<PitchSet12>(legacyMask | (1u << legacy.intervals[i]));
    assert(legacyMask == definition->intervalMask);
  }
  assert(scaleDefinition(10) == nullptr);

  HarmonySnapshot snapshot = validSnapshot();
  assert(validateHarmonySnapshot(snapshot));
  snapshot.key.materialId = 0;
  assert(!validateHarmonySnapshot(snapshot));
  snapshot = validSnapshot();
  snapshot.durationSteps = 0;
  assert(!validateHarmonySnapshot(snapshot));
  snapshot = validSnapshot();
  snapshot.base.pitchMask |= 0x1000u;
  assert(!validateHarmonySnapshot(snapshot));
  snapshot = validSnapshot();
  snapshot.roles[static_cast<uint8_t>(HarmonicRole::Lead)].preferred = 1;
  assert(!validateHarmonySnapshot(snapshot));

  const HarmonySnapshot noHarmony{};
  assert(!validateHarmonySnapshot(noHarmony));
  snapshot = validSnapshot();
  snapshot.roles[static_cast<uint8_t>(HarmonicRole::Lead)] = {0, 0};
  assert(validateHarmonySnapshot(snapshot)); // Empty is intentional, not chromatic.

  HarmonyEventKey a{1, 1, 2, 300};
  HarmonyEventKey b{1, 1, 2, 300};
  assert(sameHarmonyEvent(a, b));
  b.eventOrdinal = 301;
  assert(!sameHarmonyEvent(a, b));
  b = a;
  b.materialId++;
  assert(!sameHarmonyEvent(a, b));
  b = a;
  b.publicationGeneration++;
  assert(!sameHarmonyEvent(a, b));
  b = a;
  b.transportEpoch++;
  assert(!sameHarmonyEvent(a, b));
  return 0;
}

#else
int main() {
  assert(false && "provider-neutral harmonic snapshot contract is not implemented");
  return 1;
}
#endif
