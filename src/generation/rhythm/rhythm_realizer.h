#pragma once

#include <cstdint>
#include <type_traits>

#include "../generation_context.h"
#include "relationship_resolver.h"
#include "rhythm_catalog.h"
#include "rhythm_types.h"

namespace GroovePuterRhythm {

struct RoleRhythmPlan {
  StepMask structural = 0;
  StepMask secondary = 0;
  StepMask ghosts = 0;

  // GateClass::Normal is implicit for onsets not present in these masks.
  StepMask shortGate = 0;
  StepMask heldGate = 0;
  StepMask tieGate = 0;

  StepMask accents = 0;
};

struct RhythmBarPlan {
  // Stage 2 does not own BarEvolution. Every realized bar remains a Statement;
  // Stage 6 may later apply a caller-selected trajectory transactionally.
  BarFunction function = BarFunction::Statement;
  RoleRhythmPlan roles[kRhythmRoleCount]{};
};

struct RhythmPhrasePlan {
  uint8_t barCount = 0;
  // Stage 2 never selects/pins trajectories or TransformationIntent. Keep the
  // fields explicit so the later Stage 6 extension has a stable plan surface.
  TrajectoryId trajectoryId = kNoTrajectoryId;
  RealizationLevel level = RealizationLevel::P1Canonical;
  TransformationIntent intent = TransformationIntent::Auto;
  RhythmBarPlan bars[kMaxPhraseBars]{};
};

struct RhythmRealizationRequest {
  const RhythmCatalogView* catalog = nullptr;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  uint8_t phraseBars = 0;
  RealizationLevel level = RealizationLevel::P1Canonical;
  GenerationContext generation{};

  // Reuse this identity for VARIATE semantics. nullptr establishes a new
  // deterministic identity. The realizer never mutates the supplied identity.
  const PhraseRhythmIdentity* reuseIdentity = nullptr;
};

struct RhythmRealizationResult {
  RealizationStatus status = RealizationStatus::InvalidConstraintSet;
  PhraseRhythmIdentity identity{};
  RhythmPhrasePlan plan{};
};

RhythmRealizationResult realizeRhythmPhrase(
    const RhythmRealizationRequest& request);

PhraseOccupancy structuralOccupancy(const RhythmPhrasePlan& plan);

bool planRespectsProtectedSpace(const RhythmArchetype& archetype,
                                const RhythmPhrasePlan& plan);

bool planRespectsLaneBounds(const RhythmArchetype& archetype,
                            const RhythmPhrasePlan& plan);

static_assert(std::is_trivially_copyable<RoleRhythmPlan>::value,
              "RoleRhythmPlan must remain fixed-capacity");
static_assert(std::is_trivially_copyable<RhythmPhrasePlan>::value,
              "RhythmPhrasePlan must remain fixed-capacity");
static_assert(sizeof(RhythmPhrasePlan) <= 640,
              "RhythmPhrasePlan exceeded the Stage 2 bounded RAM target");

}  // namespace GroovePuterRhythm
