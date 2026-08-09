#include "bar_evolution.h"

#include <cstddef>

namespace GroovePuterRhythm {
namespace {

uint8_t bitCount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

bool validBarFunction(BarFunction function) {
  return static_cast<uint8_t>(function) <
         static_cast<uint8_t>(BarFunction::Count);
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

StepMask anchorMask(const LaneGrammar& lane) {
  return static_cast<StepMask>(lane.immutableAnchors |
                               lane.canonicalAnchors);
}

StepMask protectedMask(const RhythmArchetype& archetype,
                       RhythmRole role) {
  StepMask mask = 0;
  const RhythmRoleMask roleMask = rhythmRoleBit(role);
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    const ProtectedSpace& space = archetype.protectedSpaces[i];
    if (space.affectedRoles & roleMask) {
      mask = static_cast<StepMask>(mask | space.steps);
    }
  }
  return mask;
}

StepMask allOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural |
                               role.secondary |
                               role.ghosts);
}

uint16_t ornamentCount(const RhythmPhrasePlan& plan, uint8_t bar) {
  uint16_t count = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    count += bitCount16(plan.bars[bar].roles[role].ghosts);
  }
  return count;
}

void recomputeRoleGates(const LaneGrammar& lane, RoleRhythmPlan& role) {
  const StepMask onsets = allOnsets(role);
  const StepMask explicitGateSites = static_cast<StepMask>(
      lane.shortGate | lane.heldGate | lane.tieGate);
  role.heldGate = static_cast<StepMask>(onsets & lane.heldGate);
  role.tieGate = static_cast<StepMask>(onsets & lane.tieGate);
  role.shortGate = static_cast<StepMask>(
      (onsets & lane.shortGate) |
      (role.ghosts & ~explicitGateSites));
}

void recomputeBarGates(const RhythmArchetype& archetype,
                       RhythmBarPlan& bar) {
  for (uint8_t laneIndex = 0;
       laneIndex < archetype.laneCount;
       ++laneIndex) {
    const LaneGrammar& lane = archetype.lanes[laneIndex];
    recomputeRoleGates(
        lane, bar.roles[static_cast<uint8_t>(lane.role)]);
  }
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

uint32_t evolutionCoordinate(uint8_t bar,
                             RhythmRole role,
                             uint8_t step,
                             uint8_t pass) {
  return (static_cast<uint32_t>(bar) << 24u) |
         (static_cast<uint32_t>(static_cast<uint8_t>(role)) << 16u) |
         (static_cast<uint32_t>(step) << 8u) |
         pass;
}

bool addGhostCue(const RhythmArchetype& archetype,
                 RhythmPhrasePlan& plan,
                 uint8_t bar,
                 uint32_t seed,
                 uint8_t pass,
                 bool lateOnly) {
  if (bar >= plan.barCount ||
      ornamentCount(plan, bar) >= archetype.density.ornamentMax) {
    return false;
  }

  constexpr StepMask kLateSteps = static_cast<StepMask>(
      stepBit(12) | stepBit(13) | stepBit(14) | stepBit(15));
  int bestLane = -1;
  int bestStep = -1;
  uint32_t bestRank = 0;

  for (uint8_t laneIndex = 0;
       laneIndex < archetype.laneCount;
       ++laneIndex) {
    const LaneGrammar& lane = archetype.lanes[laneIndex];
    RoleRhythmPlan& role =
        plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
    if (bitCount16(role.ghosts) >= lane.ornamentMax) continue;

    StepMask candidates = static_cast<StepMask>(
        (lane.preferred | lane.optional) &
        ~allOnsets(role) &
        ~lane.forbidden &
        ~protectedMask(archetype, lane.role));
    if (lateOnly) candidates = static_cast<StepMask>(candidates & kLateSteps);

    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (!(candidates & stepBit(step))) continue;
      const uint32_t rank = deterministicValue(
          seed, evolutionCoordinate(bar, lane.role, step, pass));
      if (bestLane < 0 || rank > bestRank) {
        bestLane = laneIndex;
        bestStep = step;
        bestRank = rank;
      }
    }
  }

  if (bestLane < 0 || bestStep < 0) return false;
  const LaneGrammar& lane = archetype.lanes[bestLane];
  RhythmPhrasePlan trial = plan;
  RoleRhythmPlan& role =
      trial.bars[bar].roles[static_cast<uint8_t>(lane.role)];
  role.ghosts = static_cast<StepMask>(role.ghosts |
                                      stepBit(static_cast<uint8_t>(bestStep)));
  recomputeRoleGates(lane, role);
  if (!evolvedPlanValid(archetype, trial)) return false;
  plan = trial;
  return true;
}

struct DropCandidate {
  int lane = -1;
  int step = -1;
  uint32_t rank = 0;
};

DropCandidate bestDropCandidate(const RhythmArchetype& archetype,
                                const RhythmPhrasePlan& plan,
                                uint8_t bar,
                                uint32_t seed,
                                uint8_t pass,
                                const StepMask* attempted,
                                bool secondary) {
  DropCandidate best{};
  for (uint8_t laneIndex = 0;
       laneIndex < archetype.laneCount;
       ++laneIndex) {
    const LaneGrammar& lane = archetype.lanes[laneIndex];
    const uint8_t roleIndex = static_cast<uint8_t>(lane.role);
    const RoleRhythmPlan& role = plan.bars[bar].roles[roleIndex];
    const StepMask candidates = secondary
        ? static_cast<StepMask>(role.secondary & ~attempted[roleIndex])
        : static_cast<StepMask>(role.structural & ~anchorMask(lane) &
                                ~attempted[roleIndex]);

    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (!(candidates & stepBit(step))) continue;
      const uint32_t rank = deterministicValue(
          seed, evolutionCoordinate(bar, lane.role, step, pass));
      if (best.lane < 0 || rank > best.rank) {
        best.lane = laneIndex;
        best.step = step;
        best.rank = rank;
      }
    }
  }
  return best;
}

bool dropOneStructuralEvent(const RhythmArchetype& archetype,
                            RhythmPhrasePlan& plan,
                            uint8_t bar,
                            uint32_t seed,
                            uint8_t pass) {
  if (bar >= plan.barCount) return false;

  StepMask attemptedSecondary[kRhythmRoleCount]{};
  StepMask attemptedStructural[kRhythmRoleCount]{};
  const uint16_t maxCandidates = static_cast<uint16_t>(
      archetype.laneCount * kStepsPerBar * 2u);

  for (uint16_t attempt = 0; attempt < maxCandidates; ++attempt) {
    // Secondary events are a strict lower-authority removal class. Exhaust
    // every legal secondary candidate before considering structural events;
    // deterministic rank only orders candidates inside the same class.
    DropCandidate candidate = bestDropCandidate(
        archetype, plan, bar, seed, pass,
        attemptedSecondary, true);
    bool secondary = candidate.lane >= 0;
    if (!secondary) {
      candidate = bestDropCandidate(
          archetype, plan, bar, seed, pass,
          attemptedStructural, false);
    }

    if (candidate.lane < 0 || candidate.step < 0) return false;
    const LaneGrammar& lane = archetype.lanes[candidate.lane];
    const uint8_t roleIndex = static_cast<uint8_t>(lane.role);
    const StepMask bit = stepBit(static_cast<uint8_t>(candidate.step));
    if (secondary) {
      attemptedSecondary[roleIndex] = static_cast<StepMask>(
          attemptedSecondary[roleIndex] | bit);
    } else {
      attemptedStructural[roleIndex] = static_cast<StepMask>(
          attemptedStructural[roleIndex] | bit);
    }

    RhythmPhrasePlan trial = plan;
    RoleRhythmPlan& role = trial.bars[bar].roles[roleIndex];
    if (secondary) {
      role.secondary = static_cast<StepMask>(role.secondary & ~bit);
    } else {
      role.structural = static_cast<StepMask>(role.structural & ~bit);
    }
    recomputeRoleGates(lane, role);
    if (!evolvedPlanValid(archetype, trial)) continue;
    plan = trial;
    return true;
  }
  return false;
}

void clearGhosts(const RhythmArchetype& archetype,
                 RhythmPhrasePlan& plan,
                 uint8_t bar) {
  for (uint8_t laneIndex = 0;
       laneIndex < archetype.laneCount;
       ++laneIndex) {
    const LaneGrammar& lane = archetype.lanes[laneIndex];
    RoleRhythmPlan& role =
        plan.bars[bar].roles[static_cast<uint8_t>(lane.role)];
    role.ghosts = 0;
    recomputeRoleGates(lane, role);
  }
}

bool ghostAddsAllowed(const MutationBudget& budget) {
  return budget.maxAdds != 0 &&
         (budget.flags & (AllowOptionalAdds | AllowGhostConversion));
}

bool applyBarFunction(const RhythmArchetype& archetype,
                      RhythmPhrasePlan& plan,
                      uint8_t bar,
                      BarFunction function,
                      uint32_t seed) {
  if (bar >= plan.barCount || !validBarFunction(function)) return false;
  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(plan.level)];

  switch (function) {
    case BarFunction::Statement:
      break;

    case BarFunction::Response:
      // Stage 6/6.1 v1: Response is metadata-only. The underlying bar is the
      // independently realized base bar. A future response topology transform
      // requires its own evidence, budget and acceptance contract.
      break;

    case BarFunction::Repeat:
      if (bar == 0) return false;
      plan.bars[bar] = plan.bars[bar - 1];
      break;

    case BarFunction::RepeatWithGhosts:
      if (bar == 0) return false;
      plan.bars[bar] = plan.bars[bar - 1];
      if (ghostAddsAllowed(budget)) {
        addGhostCue(archetype, plan, bar, seed, 0, false);
      }
      break;

    case BarFunction::Reduction:
      clearGhosts(archetype, plan, bar);
      for (uint8_t drop = 0; drop < budget.maxDrops; ++drop) {
        if (!dropOneStructuralEvent(archetype, plan, bar, seed, drop)) break;
      }
      break;

    case BarFunction::Build:
      if (ghostAddsAllowed(budget)) {
        for (uint8_t add = 0; add < budget.maxAdds; ++add) {
          if (!addGhostCue(archetype, plan, bar, seed, add, false)) break;
        }
      }
      break;

    case BarFunction::Turnaround:
      if (ghostAddsAllowed(budget)) {
        for (uint8_t add = 0; add < budget.maxAdds; ++add) {
          if (!addGhostCue(archetype, plan, bar, seed, add, true)) break;
        }
      }
      break;

    case BarFunction::Break:
      clearGhosts(archetype, plan, bar);
      for (uint8_t drop = 0; drop < budget.maxDrops; ++drop) {
        if (!dropOneStructuralEvent(archetype, plan, bar,
                                    seed ^ 0x42524541u, drop)) {
          break;
        }
      }
      break;

    case BarFunction::Return:
      if (bar == 0) return false;
      plan.bars[bar] = plan.bars[0];
      break;

    case BarFunction::Count:
      return false;
  }

  plan.bars[bar].function = function;
  recomputeBarGates(archetype, plan.bars[bar]);
  return evolvedPlanValid(archetype, plan);
}

}  // namespace

bool evolvedPlanValid(const RhythmArchetype& archetype,
                      const RhythmPhrasePlan& plan) {
  if (plan.barCount == 0 ||
      plan.barCount > kMaxPhraseBars ||
      !validLevel(plan.level)) {
    return false;
  }
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    if (!validBarFunction(plan.bars[bar].function)) return false;
  }

  RhythmPhrasePlan normalized = plan;
  normalized.trajectoryId = kNoTrajectoryId;
  normalized.intent = TransformationIntent::Auto;
  for (uint8_t bar = 0; bar < normalized.barCount; ++bar) {
    normalized.bars[bar].function = BarFunction::Statement;
  }

  if (!planRespectsProtectedSpace(archetype, normalized) ||
      !planRespectsLaneBounds(archetype, normalized)) {
    return false;
  }
  return hardRelationshipsSatisfied(archetype,
                                    structuralOccupancy(normalized));
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

  // Cheap request/archetype checks happen before realization. Full catalog
  // validation is intentionally delegated to realizeRhythmPhrase(), so the
  // Stage 6 wrapper does not scan the entire catalog twice per request.
  const RhythmArchetype* archetype =
      archetypeFor(*request.catalog, request.archetypeId);
  if (!archetype ||
      !(archetype->allowedPhraseBars & phraseBarsBit(request.phraseBars))) {
    result.status = BarEvolutionStatus::InvalidRequest;
    return result;
  }

  RhythmRealizationRequest baseRequest{};
  baseRequest.catalog = request.catalog;
  baseRequest.archetypeId = request.archetypeId;
  baseRequest.phraseBars = request.phraseBars;
  baseRequest.level = request.level;
  baseRequest.generation = request.generation;
  baseRequest.reuseIdentity = request.reuseIdentity;

  const RhythmRealizationResult base = realizeRhythmPhrase(baseRequest);
  result.realizationStatus = base.status;
  if (base.status != RealizationStatus::Ok &&
      base.status != RealizationStatus::ValidButSparse) {
    result.status = BarEvolutionStatus::BaseRealizationFailed;
    return result;
  }

  // At this point the base realizer has validated the supplied catalog.
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
    if (!applyBarFunction(*archetype, evolvedPlan, bar,
                          trajectory->bars[bar], barSeed)) {
      result.status = BarEvolutionStatus::EvolutionInvalid;
      return result;
    }
  }

  if (!evolvedPlanValid(*archetype, evolvedPlan)) {
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