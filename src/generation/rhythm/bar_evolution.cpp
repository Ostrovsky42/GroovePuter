#include "bar_evolution.h"

namespace GroovePuterRhythm {
namespace {

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

const RhythmArchetype* archetypeFor(const RhythmCatalogView& catalog,
                                    RhythmArchetypeId id) {
  for (uint16_t i = 0; i < catalog.archetypeCount; ++i) {
    if (catalog.archetypes[i].id == id) return &catalog.archetypes[i];
  }
  return nullptr;
}

const BarTrajectory* trajectoryFor(const RhythmCatalogView& catalog,
                                   TrajectoryId id) {
  for (uint8_t i = 0; i < catalog.trajectoryCount; ++i) {
    if (catalog.trajectories[i].id == id) return &catalog.trajectories[i];
  }
  return nullptr;
}

bool trajectoryRefEligible(const RhythmCatalogView& catalog,
                           const TrajectoryRef& ref,
                           uint8_t phraseBars,
                           RealizationLevel level,
                           TrajectoryId requested) {
  if (requested != kNoTrajectoryId && ref.id != requested) return false;
  if (!(ref.allowedLevels & realizationLevelBit(level))) return false;
  const BarTrajectory* trajectory = trajectoryFor(catalog, ref.id);
  return trajectory && trajectory->barCount == phraseBars;
}

const BarTrajectory* selectTrajectory(const BarEvolutionRequest& request,
                                      const RhythmArchetype& archetype) {
  uint16_t totalWeight = 0;
  for (uint8_t i = 0; i < archetype.trajectoryCount; ++i) {
    const TrajectoryRef& ref = archetype.trajectories[i];
    if (trajectoryRefEligible(*request.catalog, ref,
                              request.phraseBars, request.level,
                              request.requestedTrajectoryId)) {
      totalWeight = static_cast<uint16_t>(totalWeight + ref.weight);
    }
  }
  if (!totalWeight) return nullptr;

  // Selection salts are >= 0x100 because phraseBars >= 1. Evolution salts
  // below use uint8_t TrajectoryId values (<= 0xFF). Keeping those two salt
  // spaces disjoint is part of the deterministic domain contract.
  const uint32_t salt =
      (static_cast<uint32_t>(request.phraseBars) << 8u) |
      static_cast<uint8_t>(request.level);
  const uint32_t seed = deriveGenerationSeed(
      request.generation, archetype.id,
      GenerationDomain::BarEvolution, salt);
  uint16_t pick = static_cast<uint16_t>(
      deterministicValue(seed, request.generation.phraseOrdinal) %
      totalWeight);

  for (uint8_t i = 0; i < archetype.trajectoryCount; ++i) {
    const TrajectoryRef& ref = archetype.trajectories[i];
    if (!trajectoryRefEligible(*request.catalog, ref,
                               request.phraseBars, request.level,
                               request.requestedTrajectoryId)) {
      continue;
    }
    if (pick < ref.weight) return trajectoryFor(*request.catalog, ref.id);
    pick = static_cast<uint16_t>(pick - ref.weight);
  }
  return nullptr;
}

}  // namespace

bool evolvedPlanValid(const RhythmArchetype& archetype,
                      const RhythmPhrasePlan& plan) {
  return rhythmMutationPlanValid(archetype, plan);
}

BarEvolutionResult evolveRhythmPhrase(const BarEvolutionRequest& request) {
  BarEvolutionResult result{};
  if (!request.catalog ||
      request.archetypeId == kNoArchetypeId ||
      request.phraseBars == 0 ||
      request.phraseBars > kMaxPhraseBars ||
      !validLevel(request.level)) {
    result.status = BarEvolutionStatus::InvalidRequest;
    return result;
  }

  // Stage 2 owns full catalog/archetype/phrase-length validation. Do not
  // dereference catalog arrays in this wrapper before realizeRhythmPhrase()
  // has accepted the request.
  RhythmRealizationRequest baseRequest{};
  baseRequest.catalog = request.catalog;
  baseRequest.archetypeId = request.archetypeId;
  baseRequest.phraseBars = request.phraseBars;
  baseRequest.level = request.level;
  baseRequest.generation = request.generation;
  baseRequest.structuralDensityTarget = request.structuralDensityTarget;
  baseRequest.reuseIdentity = request.reuseIdentity;

  const RhythmRealizationResult base = realizeRhythmPhrase(baseRequest);
  result.realizationStatus = base.status;
  if (base.status != RealizationStatus::Ok &&
      base.status != RealizationStatus::ValidButSparse) {
    result.status = BarEvolutionStatus::BaseRealizationFailed;
    return result;
  }

  // A successful base realization proves the catalog, archetype id and
  // phrase-bars contract were validated by Stage 2. The lookup is safe now.
  const RhythmArchetype* archetype =
      archetypeFor(*request.catalog, request.archetypeId);
  if (!archetype) {
    result.status = BarEvolutionStatus::BaseRealizationFailed;
    return result;
  }

  const BarTrajectory* trajectory = selectTrajectory(request, *archetype);
  if (!trajectory) {
    result.status = BarEvolutionStatus::NoEligibleTrajectory;
    return result;
  }

  RhythmPhrasePlan evolvedPlan = base.plan;
  evolvedPlan.trajectoryId = trajectory->id;

  // TrajectoryId is uint8_t today, so this salt remains disjoint from the
  // >=0x100 selection salt above.
  const uint32_t evolutionSeed = deriveGenerationSeed(
      request.generation, archetype->id,
      GenerationDomain::BarEvolution, trajectory->id);
  for (uint8_t bar = 0; bar < trajectory->barCount; ++bar) {
    const uint32_t barSeed = deterministicValue(evolutionSeed, bar);
    if (!applyRhythmBarFunctionMutation(
            *archetype, evolvedPlan, bar, trajectory->bars[bar], barSeed)) {
      result.status = BarEvolutionStatus::EvolutionInvalid;
      return result;
    }
  }

  if (!rhythmMutationPlanValid(*archetype, evolvedPlan)) {
    result.status = BarEvolutionStatus::EvolutionInvalid;
    return result;
  }

  result.identity = base.identity;
  result.plan = evolvedPlan;
  result.trajectoryId = trajectory->id;
  result.status = BarEvolutionStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
