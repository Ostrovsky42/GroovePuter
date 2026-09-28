#pragma once
#ifndef GROOVEPUTER_DSP_DEVELOPMENT_SEMANTIC_ADAPTER_H
#define GROOVEPUTER_DSP_DEVELOPMENT_SEMANTIC_ADAPTER_H

#include "development_semantics.h"
#include "musical_development.h"

namespace GroovePuterDevelopmentSemantic {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct RelationAssessmentInput {
  ReferenceRole reference = ReferenceRole::Source;
  const Buffer* material = nullptr;
  ClaimEvidenceStatus requiredPreservation = ClaimEvidenceStatus::Unknown;
};

struct SemanticAssessmentInput {
  // These are explicit summaries of the required claims for each independent
  // question. The adapter never manufactures them from a mutation count.
  ClaimEvidenceStatus lineagePreservation = ClaimEvidenceStatus::Unknown;
  ClaimEvidenceStatus genreRequirements = ClaimEvidenceStatus::Unknown;
  ClaimEvidenceStatus operationRequirements = ClaimEvidenceStatus::Unknown;

  RelationAssessmentInput relations[kReferenceFactCount] = {
      {ReferenceRole::Source, nullptr, ClaimEvidenceStatus::Unknown},
      {ReferenceRole::Predecessor, nullptr, ClaimEvidenceStatus::Unknown},
      {ReferenceRole::ReturnTarget, nullptr, ClaimEvidenceStatus::Unknown},
  };

  bool trajectoryContextAvailable = false;
  TrajectoryRole trajectory = TrajectoryRole::Unknown;
};

inline LineageStatus lineageFromClaims(ClaimEvidenceStatus status) {
  switch (status) {
    case ClaimEvidenceStatus::Pass:
      return LineageStatus::Continues;
    case ClaimEvidenceStatus::Fail:
      return LineageStatus::NewIdea;
    default:
      return LineageStatus::Unknown;
  }
}

inline GenreStatus genreFromClaims(ClaimEvidenceStatus status) {
  switch (status) {
    case ClaimEvidenceStatus::Pass:
      return GenreStatus::Allowed;
    case ClaimEvidenceStatus::Fail:
      return GenreStatus::Violation;
    default:
      return GenreStatus::Unknown;
  }
}

inline OperationConformance operationFromClaims(ClaimEvidenceStatus status) {
  switch (status) {
    case ClaimEvidenceStatus::Pass:
      return OperationConformance::Honored;
    case ClaimEvidenceStatus::Fail:
      return OperationConformance::Violated;
    default:
      return OperationConformance::Unknown;
  }
}

inline StateRelation relationFromReference(
    const Buffer& candidate,
    const RelationAssessmentInput& input) {
  if (input.material == nullptr) return StateRelation::NotApplicable;
  if (RuntimePhraseEdit::same(candidate, *input.material)) {
    return StateRelation::Exact;
  }
  if (input.requiredPreservation == ClaimEvidenceStatus::Pass) {
    return StateRelation::Variation;
  }
  // A failed or unknown preservation claim does not by itself establish what
  // the positive state relation is. Keep the relation UNKNOWN and let LINEAGE
  // independently report NEW_IDEA when its own required claims fail.
  return StateRelation::Unknown;
}

inline bool hasPrimaryDownbeatOnset(const Buffer& buffer) {
  for (uint16_t i = 0; i < buffer.count; ++i) {
    if (buffer.events[i].startTick == 0) return true;
  }
  return false;
}

inline bool operationPromisesContour(
    GroovePuterDevelopment::TransformationKind transformation) {
  using GroovePuterDevelopment::TransformationKind;
  return transformation == TransformationKind::Revoice ||
         transformation == TransformationKind::Hold ||
         transformation == TransformationKind::Connect ||
         transformation == TransformationKind::Move;
}

inline void setCapability(SemanticFacts& facts,
                          CapabilityClaim claim,
                          CapabilityStatus status) {
  for (uint8_t i = 0; i < kCapabilityFactCount; ++i) {
    if (facts.capabilities[i].claim == claim) {
      facts.capabilities[i].status = status;
      return;
    }
  }
}

// D0-C adapter rule:
// - explicit claim summaries establish positive semantic facts;
// - legacy evidence may add a narrowly proven hard failure/capability fact;
// - legacy IdeaClassification / GenreResult / DevelopmentDisposition are never
//   imported as semantic authority.
inline SemanticFacts adaptLegacyEvidence(
    const Buffer& source,
    const Buffer& candidate,
    const GroovePuterDevelopment::DevelopmentEvidence& evidence,
    const GroovePuterDevelopment::DevelopmentRequest& request,
    const SemanticAssessmentInput& input = {}) {
  SemanticFacts facts{};

  facts.lineage = lineageFromClaims(input.lineagePreservation);
  facts.genre = genreFromClaims(input.genreRequirements);
  facts.operation = operationFromClaims(input.operationRequirements);
  facts.trajectory = input.trajectoryContextAvailable
      ? input.trajectory
      : TrajectoryRole::Unknown;

  for (uint8_t i = 0; i < kReferenceFactCount; ++i) {
    facts.relations[i].reference = input.relations[i].reference;
    facts.relations[i].result =
        relationFromReference(candidate, input.relations[i]);
  }

  using GroovePuterDevelopment::TransformationKind;
  using GroovePuterDevelopment::TriState;

  // Current 0.9.13 explicitly has no authoritative tonal-root context for
  // EXTEND. Preserve that as capability information, not musical failure.
  if (request.transformation == TransformationKind::Extend) {
    setCapability(facts, CapabilityClaim::HarmonicRootPreservation,
                  CapabilityStatus::Unavailable);
  } else if (evidence.harmony.rootPreserved != TriState::Unknown) {
    setCapability(facts, CapabilityClaim::HarmonicRootPreservation,
                  CapabilityStatus::Available);
  }

  if (operationPromisesContour(request.transformation)) {
    if (evidence.bass.contourPreserved != TriState::Unknown) {
      setCapability(facts, CapabilityClaim::ContourPreservation,
                    CapabilityStatus::Available);
    }

    // A failed promised contour is an operation-conformance fact. It must not
    // silently become a genre violation.
    if (evidence.bass.contourPreserved == TriState::Fail) {
      facts.operation = OperationConformance::Violated;
    } else if (evidence.bass.contourPreserved == TriState::Pass &&
               facts.operation == OperationConformance::Unknown) {
      facts.operation = OperationConformance::Honored;
    }
  }

  // D0-D1: RuntimeSynthEventBuffer can authoritatively answer only the
  // mechanical question "is there an onset at startTick == 0?". That
  // observable is not metric hierarchy, The One, Funk pocket, or genre
  // validity. The capability remains available, but it does not write GENRE.
  setCapability(facts, CapabilityClaim::PrimaryDownbeatOnsetPresence,
                CapabilityStatus::Available);

  // Keep SOURCE in the stable adapter signature. D0-D1 deliberately refuses
  // to manufacture a higher-level verdict from source/candidate downbeat
  // presence alone.
  (void)source;

  return facts;
}

}  // namespace GroovePuterDevelopmentSemantic

#endif  // GROOVEPUTER_DSP_DEVELOPMENT_SEMANTIC_ADAPTER_H
