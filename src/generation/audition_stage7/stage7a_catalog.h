#pragma once

#include <cstdint>

#include "../rhythm/rhythm_catalog.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {

enum class Candidate : uint8_t {
  StaggeredMachine = 0,
  CrossCycle,
  BreakHalfstep,
  RockPush,
  HalfbackControl,
  Count,
};

enum class EvidenceClass : uint8_t {
  MultiProvenanceReview = 0,
  SingleRootChallenger,
  SingleRootControl,
};

struct Definition {
  Candidate key = Candidate::StaggeredMachine;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  const char* name = "staggered_machine";
  const char* atlasCandidate = "HARD_02";
  EvidenceClass evidence = EvidenceClass::MultiProvenanceReview;
  uint16_t suggestedBpm = 120;
};

// Temporary listening-only catalog. These archetypes are deliberately outside
// ReferenceVocabulary and are never persisted as runtime vocabulary IDs.
const RhythmCatalogView& catalog();
uint8_t definitionCount();
const Definition& definition(uint8_t index);
const char* evidenceName(EvidenceClass evidence);

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
