#pragma once
#ifndef GROOVEPUTER_DSP_P0_PRESERVATION_TYPES_H
#define GROOVEPUTER_DSP_P0_PRESERVATION_TYPES_H

#include <cstdint>
#include <type_traits>

#include "development_semantics.h"

// D1-C value types only (no behavior, no dependency on the Development
// transformation code), so miniacid_engine.h can name them without pulling the
// evaluator -- or a circular include -- into the engine header.
namespace GroovePuterDevelopmentSemantic {

struct P0PreservationAssessment {
  bool available = false;
  ClaimEvidenceStatus r1BarExtent = ClaimEvidenceStatus::Unknown;
  ClaimEvidenceStatus r2BassOnsetTopology = ClaimEvidenceStatus::Unknown;
  ClaimEvidenceStatus r3PitchClassAtOnset = ClaimEvidenceStatus::Unknown;
  // Aggregate: Pass or Unknown, NEVER Fail.
  ClaimEvidenceStatus lineageSummary = ClaimEvidenceStatus::Unknown;
  // Relation to the immediate operation source only (ReferenceRole::Predecessor).
  StateRelation predecessorRelation = StateRelation::Unknown;
};

static_assert(std::is_trivially_copyable<P0PreservationAssessment>::value,
              "P0PreservationAssessment must stay a bounded value object");
static_assert(sizeof(P0PreservationAssessment) <= 8,
              "P0PreservationAssessment exceeded its bounded budget");

// Result of observing one Development operation. Observational only: it never
// influences candidate generation, the publish decision, NEXT eligibility or
// publication.
struct DevelopmentSemanticObservation {
  // true iff the generated-origin P0 provider could evaluate this operation.
  bool available = false;
  // Origin known, and CURRENT's exact version differs from originPatternVersion
  // ("changed since origin"). Evidence only -- never a verdict.
  bool currentChangedSinceOrigin = false;
  P0PreservationAssessment preservation{};
  SemanticFacts facts{};
};

static_assert(std::is_trivially_copyable<DevelopmentSemanticObservation>::value,
              "DevelopmentSemanticObservation must stay a bounded value object");

}  // namespace GroovePuterDevelopmentSemantic

#endif  // GROOVEPUTER_DSP_P0_PRESERVATION_TYPES_H
