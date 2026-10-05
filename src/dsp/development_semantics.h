#pragma once
#ifndef GROOVEPUTER_DSP_DEVELOPMENT_SEMANTICS_H
#define GROOVEPUTER_DSP_DEVELOPMENT_SEMANTICS_H

#include <cstdint>
#include <type_traits>

namespace GroovePuterDevelopmentSemantic {

// D0-C representation only. These values describe independent semantic facts;
// none of them owns publication, persistence, Material lifecycle, or generation.
enum class LineageStatus : uint8_t {
  Unknown = 0,
  Continues,
  NewIdea,
};

enum class ReferenceRole : uint8_t {
  Source = 0,
  Predecessor,
  ReturnTarget,
  Count,
};

enum class StateRelation : uint8_t {
  Unknown = 0,
  NotApplicable,
  Exact,
  Variation,
};

enum class TrajectoryRole : uint8_t {
  Unknown = 0,
  None,
  Repeat,
  Development,
  Break,
  Return,
};

enum class GenreStatus : uint8_t {
  Unknown = 0,
  Allowed,
  Violation,
};

enum class OperationConformance : uint8_t {
  Unknown = 0,
  Honored,
  Violated,
};

enum class CapabilityStatus : uint8_t {
  Unknown = 0,
  Available,
  Unavailable,
};

// Claim evidence is intentionally not boolean. D0-L7 requires unsupported
// evidence to remain local instead of collapsing into failure.
enum class ClaimEvidenceStatus : uint8_t {
  Unknown = 0,
  Pass,
  Fail,
  NotRequired,
  NotApplicable,
};

enum class CapabilityClaim : uint8_t {
  HarmonicRootPreservation = 0,
  RhythmTopology,
  ContourPreservation,
  PrimaryDownbeatOnsetPresence,
  Count,
};

struct StateRelationFact {
  ReferenceRole reference = ReferenceRole::Source;
  StateRelation result = StateRelation::NotApplicable;
};

struct CapabilityFact {
  CapabilityClaim claim = CapabilityClaim::HarmonicRootPreservation;
  CapabilityStatus status = CapabilityStatus::Unknown;
};

constexpr uint8_t kReferenceFactCount =
    static_cast<uint8_t>(ReferenceRole::Count);
constexpr uint8_t kCapabilityFactCount =
    static_cast<uint8_t>(CapabilityClaim::Count);

struct SemanticFacts {
  LineageStatus lineage = LineageStatus::Unknown;
  StateRelationFact relations[kReferenceFactCount] = {
      {ReferenceRole::Source, StateRelation::NotApplicable},
      {ReferenceRole::Predecessor, StateRelation::NotApplicable},
      {ReferenceRole::ReturnTarget, StateRelation::NotApplicable},
  };
  TrajectoryRole trajectory = TrajectoryRole::Unknown;
  GenreStatus genre = GenreStatus::Unknown;
  OperationConformance operation = OperationConformance::Unknown;
  CapabilityFact capabilities[kCapabilityFactCount] = {
      {CapabilityClaim::HarmonicRootPreservation, CapabilityStatus::Unknown},
      {CapabilityClaim::RhythmTopology, CapabilityStatus::Unknown},
      {CapabilityClaim::ContourPreservation, CapabilityStatus::Unknown},
      {CapabilityClaim::PrimaryDownbeatOnsetPresence, CapabilityStatus::Unknown},
  };
};

inline StateRelation stateRelationFor(const SemanticFacts& facts,
                                      ReferenceRole reference) {
  for (uint8_t i = 0; i < kReferenceFactCount; ++i) {
    if (facts.relations[i].reference == reference) {
      return facts.relations[i].result;
    }
  }
  return StateRelation::NotApplicable;
}

inline CapabilityStatus capabilityFor(const SemanticFacts& facts,
                                      CapabilityClaim claim) {
  for (uint8_t i = 0; i < kCapabilityFactCount; ++i) {
    if (facts.capabilities[i].claim == claim) {
      return facts.capabilities[i].status;
    }
  }
  return CapabilityStatus::Unknown;
}

static_assert(std::is_trivially_copyable<SemanticFacts>::value,
              "D0-C semantic facts must remain a bounded value object");
static_assert(sizeof(SemanticFacts) <= 32,
              "D0-C semantic facts exceeded the bounded carrier budget");

}  // namespace GroovePuterDevelopmentSemantic

#endif  // GROOVEPUTER_DSP_DEVELOPMENT_SEMANTICS_H
