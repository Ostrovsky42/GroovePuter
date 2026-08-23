#ifndef GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_REALIZER_H
#define GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_REALIZER_H

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
  // Stage 2 establishes the independently realized bar. The same realizer
  // owner also applies any later caller-planned BarFunction mutation through
  // applyRhythmBarFunctionMutation().
  BarFunction function = BarFunction::Statement;
  RoleRhythmPlan roles[kRhythmRoleCount]{};
};

struct RhythmPhrasePlan {
  uint8_t barCount = 0;
  // Stage 2 does not select/pin trajectories or TransformationIntent. Keep the
  // fields explicit so Phrase/BarEvolution planning has a stable plan surface.
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

// Canonical rhythm mutation owner for a caller-planned BarFunction. This is
// the only production executor for trajectory/reduction/build/break mutation.
// It preserves the existing deterministic BarEvolution semantics while keeping
// planning (trajectory and BarFunction choice) outside the mutation owner.
bool applyRhythmBarFunctionMutation(const RhythmArchetype& archetype,
                                    RhythmPhrasePlan& plan,
                                    uint8_t bar,
                                    BarFunction function,
                                    uint32_t seed);

// Validation surface for plans after canonical BarFunction mutation. The
// compatibility BarEvolution API delegates its historical evolvedPlanValid()
// entry point to this realizer-owned implementation.
bool rhythmMutationPlanValid(const RhythmArchetype& archetype,
                             const RhythmPhrasePlan& plan);

static_assert(std::is_trivially_copyable<RoleRhythmPlan>::value,
              "RoleRhythmPlan must remain fixed-capacity");
static_assert(std::is_trivially_copyable<RhythmPhrasePlan>::value,
              "RhythmPhrasePlan must remain fixed-capacity");
static_assert(sizeof(RhythmPhrasePlan) <= 640,
              "RhythmPhrasePlan exceeded the Stage 2 bounded RAM target");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_RHYTHM_REALIZER_H
