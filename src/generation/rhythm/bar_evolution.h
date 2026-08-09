#pragma once

#include <cstdint>
#include <type_traits>

#include "rhythm_realizer.h"

namespace GroovePuterRhythm {

enum class BarEvolutionStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  NoEligibleTrajectory,
  BaseRealizationFailed,
  EvolutionInvalid,
  Count,
};

struct BarEvolutionRequest {
  const RhythmCatalogView* catalog = nullptr;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  uint8_t phraseBars = 0;
  RealizationLevel level = RealizationLevel::P1Canonical;
  GenerationContext generation{};

  // Optional deterministic override used by tests/tools and future callers.
  // kNoTrajectoryId selects from the archetype's eligible weighted refs.
  TrajectoryId requestedTrajectoryId = kNoTrajectoryId;

  // Optional stable identity reuse for future VARIATE workflows. The supplied
  // identity is read-only; BarEvolution never mutates caller-owned state.
  const PhraseRhythmIdentity* reuseIdentity = nullptr;
};

struct BarEvolutionResult {
  BarEvolutionStatus status = BarEvolutionStatus::InvalidRequest;
  RealizationStatus realizationStatus = RealizationStatus::InvalidConstraintSet;
  TrajectoryId trajectoryId = kNoTrajectoryId;
  PhraseRhythmIdentity identity{};
  RhythmPhrasePlan plan{};
};

// Stage 6 transient core. Selects a legal trajectory, realizes a fresh bounded
// rhythm phrase and applies deterministic bar-function semantics in scratch
// value objects only. It never chooses or mutates Scene/page/bank/pattern/Song/
// PhraseCore storage destinations.
BarEvolutionResult evolveRhythmPhrase(const BarEvolutionRequest& request);

// Final invariant check for evolved plans. This accepts BarFunction/trajectory
// metadata while reusing the Stage 2 lane/protected-space/hard-relationship
// contracts for the actual event topology.
bool evolvedPlanValid(const RhythmArchetype& archetype,
                      const RhythmPhrasePlan& plan);

static_assert(std::is_trivially_copyable<BarEvolutionRequest>::value,
              "BarEvolutionRequest must remain fixed-capacity");
static_assert(std::is_trivially_copyable<BarEvolutionResult>::value,
              "BarEvolutionResult must remain fixed-capacity");

}  // namespace GroovePuterRhythm
