#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_CANDIDATE_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_CANDIDATE_H

#include <cstdint>

#include "corpus_prior.h"
#include "genre_harmonic_policy.h"
#include "harmonic_relation.h"
#include "song_variant_genome.h"

namespace GroovePuterHarmony {

constexpr uint8_t kMaxHarmonicCandidates = 32;
constexpr uint8_t kMaxTonalCandidates = 8;

struct Candidate {
  HarmonicState state{};
  HarmonicRelationFeatures features{};
  uint8_t corpusPrior = 0;
  uint8_t contextIndex = 0;
};
static_assert(sizeof(Candidate) <= 24,
              "harmonic candidate exceeded its compact payload budget");

struct CandidateList {
  Candidate items[kMaxHarmonicCandidates]{};
  uint8_t count = 0;
};

struct TonalCandidateList {
  TonalContext items[kMaxTonalCandidates]{};
  uint8_t count = 0;
};

struct CandidateRequest {
  HarmonicState current{};
  TonalContext currentContext{};
  HarmonicState home{};
  TonalContext homeContext{};
  SongVariantGenome genome{};
  HarmonicStyleId style = HarmonicStyleId::Acid;
  SectionIntent section = SectionIntent::Keep;
  uint8_t changesInSection = 0;
  uint8_t modulationsInSection = 0;
  uint8_t structuralMotionAllowed = 1;
  uint32_t runtimeQualityMask = 0;
  uint32_t runtimeScaleMask = 0;
  CanonicalFunctionToken previous2{};
  CanonicalFunctionToken previous1{};
  CorpusModelView corpus{};
};

struct CandidateScratch {
  PcVoiceLeadingScratch pcvl{};
  uint8_t roots[12]{};
  uint8_t rootCount = 0;
};

HarmonyStatus collectCandidates(const CandidateRequest& request,
                                CandidateScratch& scratch,
                                CandidateList& candidates,
                                TonalCandidateList& contexts);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_CANDIDATE_H
