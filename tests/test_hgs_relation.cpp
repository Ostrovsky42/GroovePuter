#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/harmony/harmonic_snapshot.h"
#include "src/generation/harmony/harmonic_relation.h"

using namespace GroovePuterHarmony;

namespace {
HarmonicState state(QualityId qualityId, uint8_t root, uint8_t bass = kNoBassPc) {
  HarmonicState result{};
  result.qualityId = qualityId;
  result.rootPc = root;
  result.bassPc = bass;
  const ChordQualityDefinition* definition = qualityDefinition(qualityId);
  assert(definition != nullptr);
  result.pitchMask = transposePitchSet(definition->intervalMask, root);
  return result;
}
TonalContext context(ScaleId scaleId, uint8_t tonic) {
  TonalContext result{};
  result.scaleId = scaleId;
  result.homeScaleId = scaleId;
  result.tonicPc = tonic;
  result.homeTonicPc = tonic;
  const ScaleDefinition* definition = scaleDefinition(scaleId);
  assert(definition != nullptr);
  result.scaleMask = transposePitchSet(definition->intervalMask, tonic);
  result.homeScaleMask = result.scaleMask;
  return result;
}

HarmonicRelationFeatures analyze(const HarmonicState& from,
                                 const HarmonicState& to,
                                 const TonalContext& source,
                                 const TonalContext& target) {
  PcVoiceLeadingScratch scratch{};
  HarmonicRelationFeatures result{};
  assert(analyzeRelation(from, to, source, target, scratch, result) ==
         HarmonyStatus::Ready);
  return result;
}

bool hasTag(const HarmonicRelationFeatures& features, RelationTag tag) {
  return (features.tags & relationTag(tag)) != 0;
}

TonalContext transposeContext(const TonalContext& source, uint8_t shift) {
  TonalContext result = source;
  result.tonicPc = static_cast<uint8_t>((source.tonicPc + shift) % 12);
  result.homeTonicPc = static_cast<uint8_t>((source.homeTonicPc + shift) % 12);
  result.scaleMask = transposePitchSet(source.scaleMask, shift);
  result.homeScaleMask = transposePitchSet(source.homeScaleMask, shift);
  return result;
}
}  // namespace

int main() {
  static_assert(sizeof(HarmonicRelationFeatures) <= 16);
  static_assert(sizeof(HarmonicState) <= 8);

  const TonalContext cMajor = context(1, 0);
  const TonalContext cMinor = context(0, 0);
  auto features = analyze(state(7, 0), state(6, 4), cMajor, cMajor);
  assert(features.commonToneCount == 3);
  assert(features.outOfScaleTargetTones == 0);
  assert(features.sourceOutOfScaleTones == 0);
  assert(hasTag(features, RelationTag::PrlL));
  assert(hasTag(features, RelationTag::DiatonicFunctional));
  assert(hasTag(features, RelationTag::CommonToneHigh));

  features = analyze(state(2, 7), state(1, 0), cMinor, cMinor);
  assert(features.sourceOutOfScaleTones == 1);
  assert(hasTag(features, RelationTag::DominantToTarget));
  assert(hasTag(features, RelationTag::SecondaryDominant));

  features = analyze(state(2, 4), state(1, 9), cMajor, cMajor);
  assert(hasTag(features, RelationTag::DominantToTarget));
  assert(hasTag(features, RelationTag::SecondaryDominant));

  // Diatonic C→Am receives PRL_R and functional tags simultaneously.
  const auto cToAm = analyze(state(0, 0), state(1, 9), cMajor, cMajor);
  assert(hasTag(cToAm, RelationTag::PrlR));
  assert(hasTag(cToAm, RelationTag::DiatonicFunctional));
  assert(cToAm.commonToneCount == 2);

  // A diatonic mediant is not mislabelled as a chromatic mediant.
  features = analyze(state(0, 0), state(1, 4), cMajor, cMajor);
  assert(!hasTag(features, RelationTag::ChromaticMediant));
  features = analyze(state(0, 0), state(0, 3), cMajor, cMajor);
  assert(hasTag(features, RelationTag::ChromaticMediant));
  assert(features.outOfScaleTargetTones > 0);

  features = analyze(state(0, 0), state(0, 6), cMajor, cMajor);
  assert(hasTag(features, RelationTag::TritoneRoot));
  assert(!hasTag(features, RelationTag::ChromaticMediant));
  // A shared root alone is not a PRL relation or functional movement.
  features = analyze(state(0, 0), state(7, 0), cMajor, cMajor);
  assert(!hasTag(features, RelationTag::PrlP));
  assert(!hasTag(features, RelationTag::DiatonicFunctional));
  // Extended chords retain PRL only when added tones remain context-compatible.
  features = analyze(state(7, 0), state(6, 0), cMajor, cMajor);
  assert(!hasTag(features, RelationTag::PrlP));

  // Borrowed parallel chord in one scale; modal mixture when scale context moves.
  features = analyze(state(0, 0), state(1, 5), cMajor, cMajor);
  assert(hasTag(features, RelationTag::BorrowedParallel));
  assert(!hasTag(features, RelationTag::ModalMixture));
  const TonalContext cDorian = context(2, 0);
  features = analyze(state(0, 0), state(0, 10), cMajor, cDorian);
  assert(hasTag(features, RelationTag::ModalMixture));
  assert(!hasTag(features, RelationTag::BorrowedParallel));

  const TonalContext gMajor = context(1, 7);
  features = analyze(state(0, 7), state(0, 0), cMajor, gMajor);
  assert(features.scaleCommonTones == 6);
  assert(features.scaleDifferenceCount == 2);
  assert(features.pivotCompatibility == 3);
  assert(hasTag(features, RelationTag::PivotChord));

  features = analyze(state(4, 0), state(0, 4), cMajor, cMajor);
  assert(hasTag(features, RelationTag::AugmentedPortal));
  features = analyze(state(3, 11), state(7, 0), cMajor, cMajor);
  assert(hasTag(features, RelationTag::DiminishedPassing));

  features = analyze(state(0, 0, 11), state(0, 5, 5), cMajor, cMajor);
  assert(features.bassMotionDistance == 6);
  assert(features.rootSemitoneDistance == 5);
  assert(features.rootFifthsDistance == 1);

  for (int shift = 0; shift < 12; ++shift) {
    const TonalContext shiftedMajor = context(1, static_cast<uint8_t>(shift));
    const auto shifted = analyze(
        state(0, static_cast<uint8_t>(shift)),
        state(1, static_cast<uint8_t>((shift + 9) % 12)),
        shiftedMajor, shiftedMajor);
    assert(hasTag(shifted, RelationTag::PrlR));
    assert(hasTag(shifted, RelationTag::DiatonicFunctional));
    assert(shifted.commonToneCount == cToAm.commonToneCount);
    assert(shifted.rootFifthsDistance == cToAm.rootFifthsDistance);
  }

  // Every semantic predicate remains stable when the complete musical context
  // is transposed. Several tags may describe the same edge.
  struct RelationCase {
    QualityId fromQuality;
    uint8_t fromRoot;
    QualityId toQuality;
    uint8_t toRoot;
    TonalContext source;
    TonalContext target;
    RelationTag expected;
  };
  const RelationCase cases[] = {
      {0, 0, 1, 0, cMajor, cMinor, RelationTag::PrlP},
      {0, 0, 1, 9, cMajor, cMajor, RelationTag::PrlR},
      {7, 0, 6, 4, cMajor, cMajor, RelationTag::PrlL},
      {2, 4, 1, 9, cMajor, cMajor, RelationTag::SecondaryDominant},
      {7, 0, 6, 4, cMajor, cMajor, RelationTag::CommonToneHigh},
      {0, 0, 1, 5, cMajor, cMajor, RelationTag::BorrowedParallel},
      {0, 0, 0, 10, cMajor, cDorian, RelationTag::ModalMixture},
      {0, 0, 0, 3, cMajor, cMajor, RelationTag::ChromaticMediant},
      {0, 0, 0, 6, cMajor, cMajor, RelationTag::TritoneRoot},
      {4, 0, 0, 4, cMajor, cMajor, RelationTag::AugmentedPortal},
      {3, 11, 7, 0, cMajor, cMajor, RelationTag::DiminishedPassing},
      {0, 0, 0, 7, cMajor, gMajor, RelationTag::PivotChord},
      {0, 0, 0, 7, cMajor, cMajor, RelationTag::FifthRelated},
  };
  for (const RelationCase& relation : cases) {
    const auto reference = analyze(state(relation.fromQuality, relation.fromRoot),
                                   state(relation.toQuality, relation.toRoot),
                                   relation.source, relation.target);
    assert(hasTag(reference, relation.expected));
    for (uint8_t shift = 0; shift < 12; ++shift) {
      const auto shifted = analyze(
          state(relation.fromQuality,
                static_cast<uint8_t>((relation.fromRoot + shift) % 12)),
          state(relation.toQuality,
                static_cast<uint8_t>((relation.toRoot + shift) % 12)),
          transposeContext(relation.source, shift),
          transposeContext(relation.target, shift));
      assert(shifted.tags == reference.tags);
    }
  }

  HarmonicState invalid = state(0, 0);
  invalid.pitchMask ^= 1u << 3;
  HarmonicRelationFeatures unchanged{};
  unchanged.commonToneCount = 77;
  PcVoiceLeadingScratch scratch{};
  assert(analyzeRelation(invalid, state(0, 0), cMajor, cMajor, scratch, unchanged) ==
         HarmonyStatus::Invalid);
  assert(unchanged.commonToneCount == 77);
  TonalContext invalidContext = cMajor;
  invalidContext.scaleMask ^= 1u << 1;
  assert(analyzeRelation(state(0, 0), state(0, 7), cMajor, invalidContext,
                         scratch, unchanged) == HarmonyStatus::Invalid);
  assert(unchanged.commonToneCount == 77);

  std::cout << "HGS harmonic relation features: PASS\n";
  return 0;
}
