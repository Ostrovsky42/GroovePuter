#pragma once

#include <cstdint>

#include "../rhythm/rhythm_catalog.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {

enum class Candidate : uint8_t {
  StackedQuarters = 0,
  ElectroBackskip,
  FunkHouseBridge,
  ElectroGapPush,
  Count,
};

enum class EvidenceClass : uint8_t {
  MultiProvenanceReview = 0,
  SingleRootChallenger,
  SingleRootControl,
};

struct Definition {
  Candidate key = Candidate::StackedQuarters;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  const char* name = "stacked_quarters";
  const char* atlasCandidate = "HARD_01";
  EvidenceClass evidence = EvidenceClass::SingleRootChallenger;
  uint16_t suggestedBpm = 120;
};

// Temporary listening-only catalog. Stage 7B reuses the Stage 7A harness but
// swaps in the four remaining single-root HARD clusters. These archetypes are
// deliberately outside ReferenceVocabulary and are never persisted as runtime
// vocabulary IDs.
const RhythmCatalogView& catalog();
uint8_t definitionCount();
const Definition& definition(uint8_t index);
const char* evidenceName(EvidenceClass evidence);

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
