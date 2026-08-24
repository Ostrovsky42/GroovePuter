#include "rhythm_canonical_diff.h"

namespace GroovePuterRhythm {
namespace {

enum class OnsetKind : uint8_t {
  None = 0,
  Structural,
  Secondary,
  Ghost,
};

constexpr uint8_t kNoMatchedStep = 0xFFu;
constexpr uint8_t kMaxRoleDeltas = kStepsPerBar * 2u;

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

bool validFunction(BarFunction function) {
  return static_cast<uint8_t>(function) <
         static_cast<uint8_t>(BarFunction::Count);
}

bool validIntent(TransformationIntent intent) {
  return static_cast<uint8_t>(intent) <
         static_cast<uint8_t>(TransformationIntent::Count);
}

bool archetypeShapeSafe(const RhythmArchetype& archetype) {
  return archetype.laneCount <= kMaxLanes &&
         archetype.protectedSpaceCount <= kMaxProtectedSpaces &&
         archetype.relationshipCount <= kMaxRelationships &&
         archetype.anchorTransformRuleCount <= kMaxAnchorTransformRules &&
         (archetype.laneCount == 0 || archetype.lanes) &&
         (archetype.protectedSpaceCount == 0 || archetype.protectedSpaces) &&
         (archetype.relationshipCount == 0 || archetype.relationships) &&
         (archetype.anchorTransformRuleCount == 0 ||
          archetype.anchorTransformRules);
}

bool roleMaterialValid(const RoleRhythmPlan& role) {
  const StepMask regular = static_cast<StepMask>(
      role.structural | role.secondary);
  const StepMask onsets = static_cast<StepMask>(regular | role.ghosts);
  return !(role.structural & role.secondary) &&
         !(regular & role.ghosts) &&
         !(role.accents & ~onsets);
}

bool barMaterialValid(const RhythmBarPlan& bar) {
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!roleMaterialValid(bar.roles[role])) return false;
  }
  return true;
}

OnsetKind onsetKindAt(const RoleRhythmPlan& role, uint8_t step) {
  const StepMask bit = stepBit(step);
  if (role.structural & bit) return OnsetKind::Structural;
  if (role.secondary & bit) return OnsetKind::Secondary;
  if (role.ghosts & bit) return OnsetKind::Ghost;
  return OnsetKind::None;
}

bool accentedAt(const RoleRhythmPlan& role, uint8_t step) {
  return (role.accents & stepBit(step)) != 0;
}

bool rolePlanEqual(const RoleRhythmPlan& lhs,
                   const RoleRhythmPlan& rhs) {
  return lhs.structural == rhs.structural &&
         lhs.secondary == rhs.secondary &&
         lhs.ghosts == rhs.ghosts &&
         lhs.shortGate == rhs.shortGate &&
         lhs.heldGate == rhs.heldGate &&
         lhs.tieGate == rhs.tieGate &&
         lhs.accents == rhs.accents;
}

bool barPlanEqual(const RhythmBarPlan& lhs,
                  const RhythmBarPlan& rhs) {
  if (lhs.function != rhs.function) return false;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!rolePlanEqual(lhs.roles[role], rhs.roles[role])) return false;
  }
  return true;
}

void sortRoleDeltas(RhythmMutationDelta* deltas, uint8_t count) {
  for (uint8_t i = 1; i < count; ++i) {
    const RhythmMutationDelta value = deltas[i];
    uint8_t j = i;
    while (j > 0 && rhythmMutationDeltaLess(value, deltas[j - 1])) {
      deltas[j] = deltas[j - 1];
      --j;
    }
    deltas[j] = value;
  }
}

bool appendRoleDelta(RhythmMutationDelta* deltas,
                     uint8_t& count,
                     const RhythmMutationDelta& delta) {
  if (count >= kMaxRoleDeltas || !rhythmMutationDeltaShapeValid(delta)) {
    return false;
  }
  deltas[count++] = delta;
  return true;
}

void accountDelta(const RhythmMutationDelta& delta,
                  const RhythmBarPlan& candidate,
                  CanonicalRhythmDiffStats& stats) {
  ++stats.deltaCount;
  switch (delta.operation) {
    case RhythmMutationOp::KEEP:
      break;
    case RhythmMutationOp::ADD:
      ++stats.adds;
      if (candidate.roles[static_cast<uint8_t>(delta.role)].secondary &
          stepBit(delta.targetStep)) {
        ++stats.secondaryAdds;
      }
      break;
    case RhythmMutationOp::DROP:
      ++stats.drops;
      break;
    case RhythmMutationOp::DISPLACE:
      ++stats.displacements;
      break;
    case RhythmMutationOp::ACCENT:
      ++stats.accentChanges;
      break;
    case RhythmMutationOp::GHOST:
      ++stats.ghostAdds;
      break;
    case RhythmMutationOp::Count:
      break;
  }
}

bool statsConsistent(const CanonicalRhythmDiffStats& stats) {
  if (stats.secondaryAdds > stats.adds) return false;
  const uint16_t classified = static_cast<uint16_t>(stats.adds) +
      stats.drops + stats.displacements + stats.accentChanges +
      stats.ghostAdds;
  return classified == stats.deltaCount;
}

uint8_t secondaryAddLimit(const MutationBudget& budget) {
  if (budget.maxSecondaryAdds != 0) return budget.maxSecondaryAdds;
  return (budget.flags & AllowOptionalAdds) ? budget.maxAdds : 0;
}

uint8_t directGhostAddLimit(const MutationBudget& budget) {
  if (budget.maxGhostAdds != 0) return budget.maxGhostAdds;
  return (budget.flags & AllowGhostConversion) ? budget.maxAdds : 0;
}

uint8_t ghostAddLimit(const MutationPolicy& policy,
                      RealizationLevel level) {
  const MutationBudget& budget =
      policy.level[static_cast<uint8_t>(level)];
  const uint8_t direct = directGhostAddLimit(budget);
  if (direct != 0 || level != RealizationLevel::P3Transformation) {
    return direct;
  }

  // Preserve the exact E1a cumulative ornament contract: P3 keeps P2 ghosts
  // when P3 does not declare its own ghost budget.
  return directGhostAddLimit(
      policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)]);
}

bool intentAllowed(const MutationBudget& budget,
                   TransformationIntent intent) {
  if (intent == TransformationIntent::Auto) return true;
  if (!validIntent(intent)) return false;
  return (budget.allowedIntents & transformationIntentBit(intent)) != 0;
}

bool dropClassAllowed(const MutationBudget& budget,
                      TransformationIntent intent) {
  if (budget.flags & AllowPreferredDrops) return true;
  if (intent == TransformationIntent::Reduce) {
    return (budget.flags & AllowReduction) != 0;
  }
  if (intent == TransformationIntent::Break) {
    return (budget.flags & AllowBreak) != 0;
  }
  return false;
}

}  // namespace

CanonicalRhythmDiffStatus canonicalRhythmBarDiff(
    const RhythmArchetype& archetype,
    const RhythmBarPlan& canonical,
    const RhythmBarPlan& candidate,
    BarFunction function,
    TransformationIntent intent,
    RhythmMutationDelta* deltas,
    uint16_t deltaCapacity,
    CanonicalRhythmDiffStats& stats) {
  stats = {};
  if (!archetypeShapeSafe(archetype) ||
      !validFunction(function) || !validIntent(intent) ||
      (!deltas && deltaCapacity != 0)) {
    return CanonicalRhythmDiffStatus::InvalidContext;
  }
  if (!barMaterialValid(canonical)) {
    return CanonicalRhythmDiffStatus::InvalidCanonicalMaterial;
  }
  if (!barMaterialValid(candidate)) {
    return CanonicalRhythmDiffStatus::InvalidCandidateMaterial;
  }

  CanonicalRhythmDiffStats computed{};
  uint16_t outputCount = 0;
  bool outputTooSmall = false;

  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const RhythmRole role = static_cast<RhythmRole>(roleIndex);
    const RoleRhythmPlan& sourceRole = canonical.roles[roleIndex];
    const RoleRhythmPlan& targetRole = candidate.roles[roleIndex];

    OnsetKind sourceKinds[kStepsPerBar]{};
    OnsetKind targetKinds[kStepsPerBar]{};
    bool sourceMatched[kStepsPerBar]{};
    bool targetMatched[kStepsPerBar]{};
    bool accentChanged[kStepsPerBar]{};
    uint8_t displacementTarget[kStepsPerBar]{};
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      displacementTarget[step] = kNoMatchedStep;
      sourceKinds[step] = onsetKindAt(sourceRole, step);
      targetKinds[step] = onsetKindAt(targetRole, step);

      if (sourceKinds[step] != OnsetKind::None &&
          targetKinds[step] != OnsetKind::None &&
          sourceKinds[step] != targetKinds[step]) {
        // E2c has no same-site importance/ghost conversion operation. Failing
        // closed avoids laundering such a conversion as DROP+ADD at one site.
        stats = {};
        return CanonicalRhythmDiffStatus::UnrepresentableDelta;
      }

      if (sourceKinds[step] != OnsetKind::None &&
          sourceKinds[step] == targetKinds[step]) {
        sourceMatched[step] = true;
        targetMatched[step] = true;
        accentChanged[step] =
            accentedAt(sourceRole, step) != accentedAt(targetRole, step);
      }
    }

    // Greedy bounded matching is deterministic by the already-frozen E2c
    // canonical delta order: source logical step first, then target step.
    for (uint8_t sourceStep = 0;
         sourceStep < kStepsPerBar;
         ++sourceStep) {
      if (sourceKinds[sourceStep] == OnsetKind::None ||
          sourceMatched[sourceStep]) {
        continue;
      }
      for (uint8_t targetStep = 0;
           targetStep < kStepsPerBar;
           ++targetStep) {
        if (targetKinds[targetStep] == OnsetKind::None ||
            targetMatched[targetStep] ||
            targetKinds[targetStep] != sourceKinds[sourceStep] ||
            accentedAt(sourceRole, sourceStep) !=
                accentedAt(targetRole, targetStep)) {
          continue;
        }
        const RhythmMutationDelta displacement = {
            RhythmMutationOp::DISPLACE, role, sourceStep, targetStep};
        if (!rhythmMutationDisplacementGrammarLegal(
                archetype, displacement, function, intent)) {
          continue;
        }
        sourceMatched[sourceStep] = true;
        targetMatched[targetStep] = true;
        displacementTarget[sourceStep] = targetStep;
        break;
      }
    }

    RhythmMutationDelta pending[kMaxRoleDeltas]{};
    uint8_t pendingCount = 0;
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if (displacementTarget[step] != kNoMatchedStep) {
        if (!appendRoleDelta(
                pending, pendingCount,
                {RhythmMutationOp::DISPLACE, role, step,
                 displacementTarget[step]})) {
          stats = {};
          return CanonicalRhythmDiffStatus::UnrepresentableDelta;
        }
      } else if (sourceKinds[step] != OnsetKind::None &&
                 !sourceMatched[step]) {
        if (!appendRoleDelta(
                pending, pendingCount,
                {RhythmMutationOp::DROP, role, step, kNoMutationStep})) {
          stats = {};
          return CanonicalRhythmDiffStatus::UnrepresentableDelta;
        }
      }

      if (targetKinds[step] != OnsetKind::None &&
          !targetMatched[step]) {
        const RhythmMutationOp operation =
            targetKinds[step] == OnsetKind::Ghost
                ? RhythmMutationOp::GHOST
                : RhythmMutationOp::ADD;
        if (!appendRoleDelta(
                pending, pendingCount,
                {operation, role, kNoMutationStep, step})) {
          stats = {};
          return CanonicalRhythmDiffStatus::UnrepresentableDelta;
        }
      }

      if (accentChanged[step]) {
        if (!appendRoleDelta(
                pending, pendingCount,
                {RhythmMutationOp::ACCENT, role, step, step})) {
          stats = {};
          return CanonicalRhythmDiffStatus::UnrepresentableDelta;
        }
      }
    }

    sortRoleDeltas(pending, pendingCount);
    for (uint8_t index = 0; index < pendingCount; ++index) {
      if (deltas) {
        if (outputCount < deltaCapacity) {
          deltas[outputCount] = pending[index];
        } else {
          outputTooSmall = true;
        }
      }
      accountDelta(pending[index], candidate, computed);
      ++outputCount;
    }
  }

  if (computed.deltaCount != outputCount || !statsConsistent(computed)) {
    stats = {};
    return CanonicalRhythmDiffStatus::UnrepresentableDelta;
  }
  stats = computed;
  return outputTooSmall
             ? CanonicalRhythmDiffStatus::OutputTooSmall
             : CanonicalRhythmDiffStatus::Ok;
}

bool canonicalRhythmBudgetValid(
    const MutationPolicy& policy,
    RealizationLevel level,
    TransformationIntent intent,
    const CanonicalRhythmDiffStats& stats) {
  if (!validLevel(level) || !validIntent(intent) ||
      !statsConsistent(stats)) {
    return false;
  }

  const MutationBudget& budget =
      policy.level[static_cast<uint8_t>(level)];
  if (!intentAllowed(budget, intent)) return false;

  const uint8_t regularAdds = static_cast<uint8_t>(
      stats.adds - stats.secondaryAdds);
  const uint8_t regularAddLimit =
      (budget.flags & AllowOptionalAdds) ? budget.maxAdds : 0;
  if (regularAdds > regularAddLimit ||
      stats.secondaryAdds > secondaryAddLimit(budget) ||
      stats.ghostAdds > ghostAddLimit(policy, level) ||
      stats.drops > budget.maxDrops ||
      stats.displacements > budget.maxDisplacements ||
      stats.accentChanges > budget.maxAccentChanges) {
    return false;
  }

  if (stats.drops != 0 && !dropClassAllowed(budget, intent)) return false;
  if (stats.displacements != 0 &&
      !(budget.flags & AllowOptionalDisplace)) {
    return false;
  }
  if (stats.accentChanges != 0 &&
      !(budget.flags & AllowAccentVariation)) {
    return false;
  }
  return true;
}

CanonicalRhythmCandidateValidation canonicalRhythmCandidateValid(
    const RhythmArchetype& archetype,
    const RhythmPhrasePlan& canonical,
    const RhythmPhrasePlan& candidate,
    uint8_t barIndex,
    RealizationLevel level,
    BarFunction function,
    TransformationIntent intent,
    RhythmMutationDelta* deltas,
    uint16_t deltaCapacity) {
  CanonicalRhythmCandidateValidation result{};
  if (!archetypeShapeSafe(archetype) ||
      !validLevel(level) || !validFunction(function) || !validIntent(intent) ||
      canonical.barCount == 0 || canonical.barCount > kMaxPhraseBars ||
      candidate.barCount != canonical.barCount ||
      barIndex >= canonical.barCount ||
      candidate.level != level ||
      candidate.bars[barIndex].function != function ||
      (!deltas && deltaCapacity != 0)) {
    result.diffStatus = CanonicalRhythmDiffStatus::InvalidContext;
    return result;
  }

  for (uint8_t bar = 0; bar < canonical.barCount; ++bar) {
    if (bar == barIndex) continue;
    if (!barPlanEqual(canonical.bars[bar], candidate.bars[bar])) {
      result.diffStatus = CanonicalRhythmDiffStatus::HiddenPhraseChange;
      return result;
    }
  }

  result.diffStatus = canonicalRhythmBarDiff(
      archetype, canonical.bars[barIndex], candidate.bars[barIndex],
      function, intent, deltas, deltaCapacity, result.stats);
  if (result.diffStatus != CanonicalRhythmDiffStatus::Ok) return result;

  // These remain rhythm_realizer-owned musical checks. E2b does not duplicate
  // archetype anchors, protected spaces, forbidden placement, lane density or
  // hard relationship semantics.
  result.canonicalPlanValid = rhythmMutationPlanValid(archetype, canonical);
  result.candidatePlanValid = rhythmMutationPlanValid(archetype, candidate);
  result.budgetValid = canonicalRhythmBudgetValid(
      archetype.mutation, level, intent, result.stats);
  result.legal = result.canonicalPlanValid &&
                 result.candidatePlanValid &&
                 result.budgetValid;
  return result;
}

}  // namespace GroovePuterRhythm
