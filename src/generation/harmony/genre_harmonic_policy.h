#ifndef GROOVEPUTER_GENERATION_HARMONY_GENRE_HARMONIC_POLICY_H
#define GROOVEPUTER_GENERATION_HARMONY_GENRE_HARMONIC_POLICY_H

#include <cstdint>

#include "harmonic_types.h"

struct GenreSettings;

namespace GroovePuterHarmony {

enum class HarmonicStyleId : uint8_t {
  Acid = 0,
  DetroitTechno = 1,
  Electro = 2,
  EBM = 3,
  UkGarage = 4,
  DubTechno = 5,
  Ambient = 6,
  LoFi = 7,
  Chillout = 8,
  DeepHouse = 9,
  Jungle = 10,
  Trap = 11,
  HipHop = 12,
  Footwork = 13,
  DnB = 14,
  Salsa = 15,
  Synthwave = 16,
  Count = 17,
};

struct HarmonicStyleWeights {
  uint8_t diatonic;
  uint8_t prl;
  uint8_t functional;
  uint8_t modal;
  uint8_t chromatic;
  uint8_t augmented;
  uint8_t polychord;
  uint8_t mobility;
  uint8_t home;
};

struct HarmonicFeatureWeights {
  uint8_t commonTone;
  uint8_t pcvlPenalty;
  uint8_t fifthsDistancePenalty;
  uint8_t outOfScalePenalty;
  uint8_t largeRootMotionPenalty;
  uint8_t diatonicTag;
  uint8_t prlTag;
  uint8_t functionalTag;
  uint8_t modalTag;
  uint8_t chromaticMediantTag;
  uint8_t portalTag;
  uint8_t intervalVector;
  uint8_t complexity;
  uint8_t corpus;
};

struct GenreHarmonicPolicy {
  HarmonicStyleId style;
  uint8_t qualityTier;
  uint8_t scaleTier;
  uint32_t qualityMask;
  uint32_t scaleMask;
  HarmonicStyleWeights priors;
  HarmonicFeatureWeights featureWeights;
  uint8_t maxOutsideTakeKeep;
  uint8_t maxOutsideDevelop;
  uint8_t maxOutsideBreak;
  uint8_t maxModulationsDevelop;
  uint8_t maxModulationsBreak;
  uint8_t allowPortals;
  uint8_t colorBudget;
  uint8_t changesPerSection;
  uint8_t functionalPreparation;
  uint8_t hostProfileOnly;
};

struct HarmonicCandidatePolicyFacts {
  uint8_t qualityId;
  uint8_t scaleId;
  SectionIntent section;
  uint8_t targetOutOfScaleTones;
  uint8_t changesInSection;
  uint8_t modulationsInSection;
  uint8_t colorCount;
  uint8_t usesPortal;
  uint8_t isHold;
  uint8_t structuralMotionAllowed;
  uint8_t requestsFunctionalPreparation;
};

static_assert(static_cast<uint8_t>(HarmonicStyleId::Count) == 17,
              "harmonic style IDs are append-only");
static_assert(sizeof(GenreHarmonicPolicy) <= 52,
              "genre policy must remain a compact immutable row");

const GenreHarmonicPolicy* policyFor(HarmonicStyleId style);
bool policyAllowsCandidate(const GenreHarmonicPolicy& policy,
                           const HarmonicCandidatePolicyFacts& facts);
HarmonyStatus harmonicStyleFor(const ::GenreSettings& settings,
                               HarmonicStyleId& out);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_GENRE_HARMONIC_POLICY_H
