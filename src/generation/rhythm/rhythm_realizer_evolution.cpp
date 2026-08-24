#include "rhythm_realizer.h"

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

bool validBarFunction(BarFunction function) {
  return static_cast<uint8_t>(function) <
         static_cast<uint8_t>(BarFunction::Count);
}

// E2A_CANDIDATE_PRODUCER_HELPERS_BEGIN
bool validTransformationIntent(TransformationIntent intent) {
  return static_cast<uint8_t>(intent) <
         static_cast<uint8_t>(TransformationIntent::Count);
}

bool validRealizationLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

const LaneGrammar* laneForRole(const RhythmArchetype& archetype,
                               RhythmRole role) {
  for (uint8_t laneIndex = 0;
       laneIndex < archetype.laneCount;
       ++laneIndex) {
    if (archetype.lanes[laneIndex].role == role) {
      return &archetype.lanes[laneIndex];
    }
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

StepMask structuralOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural | role.secondary);
}

uint16_t ornamentCount(const RhythmPhrasePlan& plan, uint8_t bar) {
  uint16_t count = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    count += bitCount16(plan.bars[bar].roles[role].ghosts);
  }
  return count;
}

uint16_t structuralCount(const RhythmPhrasePlan& plan, uint8_t bar) {
  uint16_t count = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    count += bitCount16(structuralOnsets(plan.bars[bar].roles[role]));
  }
  return count;
}

bool producerRequestValid(const RhythmMutationProducerRequest& request) {
  if (!request.archetype ||
      !request.canonical ||
      !request.current ||
      request.roles == 0 ||
      (request.roles & static_cast<RhythmRoleMask>(~kAllRhythmRoles)) ||
      !validBarFunction(request.function) ||
      !validTransformationIntent(request.intent) ||
      !validRealizationLevel(request.level)) {
    return false;
  }

  const RhythmArchetype& archetype = *request.archetype;
  if (archetype.laneCount == 0 ||
      archetype.laneCount > kMaxLanes ||
      !archetype.lanes ||
      archetype.protectedSpaceCount > kMaxProtectedSpaces ||
      (archetype.protectedSpaceCount != 0 && !archetype.protectedSpaces) ||
      archetype.anchorTransformRuleCount > kMaxAnchorTransformRules ||
      (archetype.anchorTransformRuleCount != 0 &&
       !archetype.anchorTransformRules)) {
    return false;
  }

  return request.canonical->barCount != 0 &&
         request.canonical->barCount <= kMaxPhraseBars &&
         request.current->barCount == request.canonical->barCount &&
         request.bar < request.current->barCount;
}

bool transformIntentPermission(const MutationBudget& budget,
                               BarFunction function,
                               TransformationIntent intent) {
  uint16_t requiredFlag = 0;
  TransformationIntent requiredIntent = TransformationIntent::Auto;
  switch (function) {
    case BarFunction::Reduction:
      requiredFlag = AllowReduction;
      requiredIntent = TransformationIntent::Reduce;
      break;
    case BarFunction::Turnaround:
      requiredFlag = AllowTurnaround;
      requiredIntent = TransformationIntent::Turnaround;
      break;
    case BarFunction::Break:
      requiredFlag = AllowBreak;
      requiredIntent = TransformationIntent::Break;
      break;
    default:
      return false;
  }

  return intent == requiredIntent &&
         (budget.flags & requiredFlag) &&
         (budget.allowedIntents & transformationIntentBit(requiredIntent));
}

bool destructiveTransformPermission(const MutationBudget& budget,
                                    BarFunction function,
                                    TransformationIntent intent) {
  return (function == BarFunction::Reduction ||
          function == BarFunction::Break) &&
         transformIntentPermission(budget, function, intent);
}

bool dropProposalPermitted(const MutationBudget& budget,
                           BarFunction function,
                           TransformationIntent intent,
                           bool ghost) {
  const bool destructiveTransform =
      destructiveTransformPermission(budget, function, intent);
  if (ghost) return destructiveTransform;
  return (budget.flags & AllowPreferredDrops) || destructiveTransform;
}

bool ghostProposalPermitted(const RhythmArchetype& archetype,
                            RealizationLevel level,
                            BarFunction function) {
  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(level)];
  if (budget.flags & AllowGhostConversion) return true;

  // Preserve the existing P3 cumulative production permission: P3 keeps the
  // P2 ghost class even when the P3 row only enables optional additions. This
  // mirrors permission, not the numeric P2/P3 ghost quota; E2b owns quota/diff.
  if (level == RealizationLevel::P3Transformation) {
    const MutationBudget& p2 = archetype.mutation.level[
        static_cast<uint8_t>(RealizationLevel::P2Variation)];
    if (p2.flags & AllowGhostConversion) return true;
  }

  // Existing BarFunction executor paths can add ghost cues from a general
  // optional-add permission for these planning functions.
  return (function == BarFunction::RepeatWithGhosts ||
          function == BarFunction::Build ||
          function == BarFunction::Turnaround) &&
         (budget.flags & AllowOptionalAdds);
}

bool targetSiteMutable(const RhythmArchetype& archetype,
                       const LaneGrammar& lane,
                       StepMask bit) {
  return (bit & (lane.preferred | lane.optional)) &&
         !(bit & (lane.immutableAnchors |
                  lane.canonicalAnchors |
                  lane.forbidden |
                  protectedMask(archetype, lane.role)));
}

bool canonicalSourcePresent(const RhythmMutationProducerRequest& request,
                            RhythmRole role,
                            StepMask bit) {
  const RoleRhythmPlan& canonicalRole =
      request.canonical->bars[request.bar].roles[static_cast<uint8_t>(role)];
  return (structuralOnsets(canonicalRole) & bit) != 0;
}

bool canonicalDropRuleAllows(const RhythmArchetype& archetype,
                             RhythmRole role,
                             StepMask bit,
                             BarFunction function,
                             TransformationIntent intent) {
  for (uint8_t index = 0;
       index < archetype.anchorTransformRuleCount;
       ++index) {
    const AnchorTransformRule& rule = archetype.anchorTransformRules[index];
    if (rule.role == role &&
        rule.barFunction == function &&
        rule.intent == intent &&
        (rule.suppressibleCanonical & bit)) {
      return true;
    }
  }
  return false;
}

bool dropSourceMutable(const RhythmMutationProducerRequest& request,
                       const LaneGrammar& lane,
                       StepMask bit) {
  const RhythmArchetype& archetype = *request.archetype;
  if (bit & (lane.immutableAnchors |
             lane.forbidden |
             protectedMask(archetype, lane.role))) {
    return false;
  }

  if (bit & lane.canonicalAnchors) {
    return canonicalSourcePresent(request, lane.role, bit) &&
           canonicalDropRuleAllows(archetype,
                                   lane.role,
                                   bit,
                                   request.function,
                                   request.intent);
  }

  return (bit & (lane.preferred | lane.optional)) != 0;
}

bool accentSourceMutable(const RhythmArchetype& archetype,
                         const LaneGrammar& lane,
                         StepMask bit) {
  return (bit & (lane.preferred | lane.optional)) &&
         !(bit & (lane.immutableAnchors |
                  lane.canonicalAnchors |
                  lane.forbidden |
                  protectedMask(archetype, lane.role)));
}

bool topologyCandidateSafe(const RhythmMutationProducerRequest& request,
                           const LaneGrammar& lane,
                           const RhythmMutationDelta& delta) {
  const RhythmPhrasePlan& current = *request.current;
  const uint8_t bar = request.bar;
  const uint8_t roleIndex = static_cast<uint8_t>(lane.role);
  const RoleRhythmPlan& role = current.bars[bar].roles[roleIndex];
  PhraseOccupancy occupancy = structuralOccupancy(current);

  const StepMask sourceBit = rhythmMutationStepValid(delta.sourceStep)
                                 ? stepBit(delta.sourceStep)
                                 : 0;
  const StepMask targetBit = rhythmMutationStepValid(delta.targetStep)
                                 ? stepBit(delta.targetStep)
                                 : 0;
  const uint8_t laneCount = bitCount16(structuralOnsets(role));
  const uint16_t totalCount = structuralCount(current, bar);

  switch (delta.operation) {
    case RhythmMutationOp::ADD:
      if (laneCount >= lane.structuralMax ||
          totalCount >= request.archetype->density.structuralMax) {
        return false;
      }
      occupancy.roleMasks[bar][roleIndex] = static_cast<StepMask>(
          occupancy.roleMasks[bar][roleIndex] | targetBit);
      break;

    case RhythmMutationOp::DROP:
      if (role.ghosts & sourceBit) return true;
      if (laneCount <= lane.structuralMin ||
          totalCount <= request.archetype->density.structuralMin) {
        return false;
      }
      occupancy.roleMasks[bar][roleIndex] = static_cast<StepMask>(
          occupancy.roleMasks[bar][roleIndex] & ~sourceBit);
      break;

    case RhythmMutationOp::DISPLACE:
      occupancy.roleMasks[bar][roleIndex] = static_cast<StepMask>(
          (occupancy.roleMasks[bar][roleIndex] & ~sourceBit) | targetBit);
      break;

    case RhythmMutationOp::ACCENT:
    case RhythmMutationOp::GHOST:
    case RhythmMutationOp::KEEP:
    case RhythmMutationOp::Count:
      return true;
  }

  return hardRelationshipsSatisfied(*request.archetype, occupancy);
}

void appendCandidate(const RhythmMutationDelta& delta,
                     RhythmMutationDelta* output,
                     uint16_t capacity,
                     RhythmMutationProducerResult& result) {
  if (!rhythmMutationDeltaShapeValid(delta)) return;
  if (result.count < capacity) {
    output[result.count++] = delta;
  } else {
    result.truncated = true;
  }
}

// E2A_CANDIDATE_PRODUCER_HELPERS_END

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
  if (!rhythmMutationPlanValid(archetype, trial)) return false;
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
    if (!rhythmMutationPlanValid(archetype, trial)) continue;
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

}  // namespace

RhythmMutationProducerResult produceRhythmMutationCandidates(
    const RhythmMutationProducerRequest& request,
    RhythmMutationDelta* output,
    uint16_t capacity) {
  RhythmMutationProducerResult result{};
  if (!output || capacity == 0 || !producerRequestValid(request)) {
    return result;
  }

  result.status = RhythmMutationProducerStatus::Ok;
  const uint16_t boundedCapacity =
      capacity < kMaxRhythmMutationDeltasPerBar
          ? capacity
          : kMaxRhythmMutationDeltasPerBar;
  const RhythmArchetype& archetype = *request.archetype;
  const MutationBudget& budget =
      archetype.mutation.level[static_cast<uint8_t>(request.level)];

  // Emit directly in the frozen E2c order: role ordinal, logical step,
  // operation ordinal, source, target. No sort/container iteration is needed.
  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const RhythmRole role = static_cast<RhythmRole>(roleIndex);
    const RhythmRoleMask roleBit = rhythmRoleBit(role);
    if (!(request.roles & roleBit) ||
        !(archetype.activeRoles & roleBit)) {
      continue;
    }

    const LaneGrammar* lane = laneForRole(archetype, role);
    if (!lane) continue;
    const RoleRhythmPlan& currentRole =
        request.current->bars[request.bar].roles[roleIndex];

    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      const StepMask bit = stepBit(step);
      const bool occupied = (allOnsets(currentRole) & bit) != 0;
      const bool nonGhost = (structuralOnsets(currentRole) & bit) != 0;

      if (!occupied &&
          (budget.flags & AllowOptionalAdds) &&
          targetSiteMutable(archetype, *lane, bit)) {
        const RhythmMutationDelta delta{
            RhythmMutationOp::ADD, role, kNoMutationStep, step};
        if (topologyCandidateSafe(request, *lane, delta)) {
          appendCandidate(delta, output, boundedCapacity, result);
        }
      }

      if (occupied &&
          dropProposalPermitted(budget, request.function, request.intent,
                                (currentRole.ghosts & bit) != 0) &&
          dropSourceMutable(request, *lane, bit)) {
        const RhythmMutationDelta delta{
            RhythmMutationOp::DROP, role, step, kNoMutationStep};
        if (topologyCandidateSafe(request, *lane, delta)) {
          appendCandidate(delta, output, boundedCapacity, result);
        }
      }

      if (nonGhost && (budget.flags & AllowOptionalDisplace)) {
        if (!(bit & lane->canonicalAnchors) ||
            canonicalSourcePresent(request, role, bit)) {
          for (uint8_t target = 0; target < kStepsPerBar; ++target) {
            const StepMask targetBit = stepBit(target);
            if (allOnsets(currentRole) & targetBit) continue;

            const RhythmMutationDelta delta{
                RhythmMutationOp::DISPLACE, role, step, target};
            if (!rhythmMutationDisplacementGrammarLegal(
                    archetype, delta, request.function, request.intent)) {
              continue;
            }
            if (topologyCandidateSafe(request, *lane, delta)) {
              appendCandidate(delta, output, boundedCapacity, result);
            }
          }
        }
      }

      if (nonGhost &&
          (budget.flags & AllowAccentVariation) &&
          accentSourceMutable(archetype, *lane, bit)) {
        const RhythmMutationDelta delta{
            RhythmMutationOp::ACCENT, role, step, step};
        appendCandidate(delta, output, boundedCapacity, result);
      }

      if (!occupied &&
          ghostProposalPermitted(archetype, request.level, request.function) &&
          targetSiteMutable(archetype, *lane, bit) &&
          bitCount16(currentRole.ghosts) < lane->ornamentMax &&
          ornamentCount(*request.current, request.bar) <
              archetype.density.ornamentMax) {
        const RhythmMutationDelta delta{
            RhythmMutationOp::GHOST, role, kNoMutationStep, step};
        appendCandidate(delta, output, boundedCapacity, result);
      }
    }
  }

  return result;
}

bool rhythmMutationPlanValid(const RhythmArchetype& archetype,
                             const RhythmPhrasePlan& plan) {
  if (plan.barCount == 0 ||
      plan.barCount > kMaxPhraseBars ||
      static_cast<uint8_t>(plan.level) >=
          static_cast<uint8_t>(RealizationLevel::Count)) {
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

bool applyRhythmBarFunctionMutation(const RhythmArchetype& archetype,
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
  return rhythmMutationPlanValid(archetype, plan);
}

}  // namespace GroovePuterRhythm
