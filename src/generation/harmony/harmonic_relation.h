#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_RELATION_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_RELATION_H

#include <cstdint>

#include "harmonic_snapshot.h"
#include "pitch_set.h"

namespace GroovePuterHarmony {

enum class RelationTag : uint8_t {
  PrlP = 0,
  PrlR = 1,
  PrlL = 2,
  DiatonicFunctional = 3,
  DominantToTarget = 4,
  SecondaryDominant = 5,
  BorrowedParallel = 6,
  ModalMixture = 7,
  ChromaticMediant = 8,
  TritoneRoot = 9,
  AugmentedPortal = 10,
  DiminishedPassing = 11,
  CommonToneHigh = 12,
  FifthRelated = 13,
  PivotChord = 14,
};

constexpr RelationTags relationTag(RelationTag tag) {
  return static_cast<RelationTags>(1u << static_cast<uint8_t>(tag));
}

enum RelationContextFlag : uint8_t {
  RelationSameScale = 1u << 0,
  RelationSameTonic = 1u << 1,
};

struct HarmonicRelationFeatures {
  uint8_t commonToneCount = 0;
  uint8_t setDifferenceCount = 0;
  uint8_t pcVoiceLeadingCost = 0;
  uint8_t intervalVectorDistance = 0;
  uint8_t rootSemitoneDistance = 0;
  uint8_t rootFifthsDistance = 0;
  uint8_t outOfScaleTargetTones = 0;
  uint8_t sourceOutOfScaleTones = 0;
  int8_t complexityDelta = 0;
  uint8_t bassMotionDistance = 0;
  RelationTags tags = 0;
  uint8_t contextFlags = 0;
  uint8_t scaleCommonTones = 0;
  uint8_t scaleDifferenceCount = 0;
  uint8_t pivotCompatibility = 0;
};

static_assert(sizeof(HarmonicRelationFeatures) == 16,
              "HGS relation feature packing changed");

HarmonyStatus analyzeRelation(const HarmonicState& from,
                              const HarmonicState& to,
                              const TonalContext& sourceContext,
                              const TonalContext& targetContext,
                              PcVoiceLeadingScratch& scratch,
                              HarmonicRelationFeatures& out);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_RELATION_H
