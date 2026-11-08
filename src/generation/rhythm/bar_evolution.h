#ifndef GROOVEPUTER_GENERATION_RHYTHM_BAR_EVOLUTION_H
#define GROOVEPUTER_GENERATION_RHYTHM_BAR_EVOLUTION_H

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

  // GF2-I4: already-resolved profile activity intent. BarEvolution never
  // recomputes it per bar; it only forwards it to the base RhythmRealizer.
  uint8_t structuralDensityTarget = kNoStructuralDensityTarget;

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

// Compatibility/planning surface. It selects the existing legal trajectory and
// derives deterministic per-bar coordinates, but actual RhythmPhrasePlan
// mutation is delegated to the canonical rhythm_realizer owner. It never
// chooses or mutates Scene/page/bank/pattern/Song/PhraseCore storage.
BarEvolutionResult evolveRhythmPhrase(const BarEvolutionRequest& request);

// Compatibility validation entry point retained for Stage 6 callers. There is
// no second validator/mutation implementation here: this delegates directly to
// rhythmMutationPlanValid() in the canonical rhythm_realizer owner.
bool evolvedPlanValid(const RhythmArchetype& archetype,
                      const RhythmPhrasePlan& plan);

static_assert(std::is_trivially_copyable<BarEvolutionRequest>::value,
              "BarEvolutionRequest must remain fixed-capacity");
static_assert(std::is_trivially_copyable<BarEvolutionResult>::value,
              "BarEvolutionResult must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_BAR_EVOLUTION_H
