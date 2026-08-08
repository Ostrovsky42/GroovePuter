#include "pattern_materializer.h"

namespace GroovePuterRhythm {
namespace {

constexpr int8_t kNoDrumVoice = -1;

uint8_t popcount16(StepMask value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<StepMask>(value & static_cast<StepMask>(value - 1u));
    ++count;
  }
  return count;
}

StepMask allOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural | role.secondary | role.ghosts);
}

bool roleMasksAreDisjoint(const RoleRhythmPlan& role) {
  return (role.structural & role.secondary) == 0 &&
         (role.structural & role.ghosts) == 0 &&
         (role.secondary & role.ghosts) == 0;
}

bool gateMasksAreDisjoint(const RoleRhythmPlan& role) {
  return (role.shortGate & role.heldGate) == 0 &&
         (role.shortGate & role.tieGate) == 0 &&
         (role.heldGate & role.tieGate) == 0;
}

uint8_t velocityFor(EventImportance importance) {
  switch (importance) {
    case EventImportance::Structural:
      return 110;
    case EventImportance::Secondary:
      return 86;
    case EventImportance::Ghost:
      return 52;
    default:
      return 100;
  }
}

bool validPlanShape(const RhythmPhrasePlan& plan) {
  if (plan.barCount != 1) return false;
  if (plan.trajectoryId != kNoTrajectoryId) return false;
  if (plan.intent != TransformationIntent::Auto) return false;
  if (plan.bars[0].function != BarFunction::Statement) return false;
  if (static_cast<uint8_t>(plan.level) >=
      static_cast<uint8_t>(RealizationLevel::Count)) {
    return false;
  }

  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const RoleRhythmPlan& role = plan.bars[0].roles[roleIndex];
    if (!roleMasksAreDisjoint(role) || !gateMasksAreDisjoint(role)) {
      return false;
    }
    const StepMask onsets = allOnsets(role);
    if ((role.accents & static_cast<StepMask>(~onsets)) != 0) return false;
    const StepMask gateIntent = static_cast<StepMask>(
        role.shortGate | role.heldGate | role.tieGate);
    if ((gateIntent & static_cast<StepMask>(~onsets)) != 0) return false;
  }
  return true;
}

PatternMaterializeStatus validateBinding(
    const RhythmPhrasePlan& plan,
    const PatternMaterializerBinding& binding) {
  bool usedDrumVoices[DrumPatternSet::kVoices]{};
  bool synthAUsed = false;
  bool synthBUsed = false;

  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const RhythmRole role = static_cast<RhythmRole>(roleIndex);
    const RhythmRoleMask roleBit = rhythmRoleBit(role);
    const bool ignored = (binding.ignoredRoles & roleBit) != 0;
    const int8_t drumVoice = binding.drumVoiceByRole[roleIndex];
    const SynthRhythmBinding& synth = binding.synthByRole[roleIndex];
    const bool drumBound = drumVoice != kNoDrumVoice;
    const bool synthBound = synth.target != SynthPatternTarget::None;

    if (drumVoice < kNoDrumVoice ||
        drumVoice >= static_cast<int8_t>(DrumPatternSet::kVoices)) {
      return PatternMaterializeStatus::InvalidBinding;
    }
    if (static_cast<uint8_t>(synth.target) >=
        static_cast<uint8_t>(SynthPatternTarget::Count)) {
      return PatternMaterializeStatus::InvalidBinding;
    }
    if (synthBound && synth.note < 0) {
      return PatternMaterializeStatus::InvalidBinding;
    }
    if (!synthBound && synth.note != -1) {
      return PatternMaterializeStatus::InvalidBinding;
    }
    if (ignored && (drumBound || synthBound)) {
      return PatternMaterializeStatus::InvalidBinding;
    }
    if (drumBound && synthBound) {
      return PatternMaterializeStatus::InvalidBinding;
    }

    if (drumBound) {
      if (usedDrumVoices[drumVoice]) {
        return PatternMaterializeStatus::InvalidBinding;
      }
      usedDrumVoices[drumVoice] = true;
    }

    if (synth.target == SynthPatternTarget::SynthA) {
      if (synthAUsed) return PatternMaterializeStatus::InvalidBinding;
      synthAUsed = true;
    } else if (synth.target == SynthPatternTarget::SynthB) {
      if (synthBUsed) return PatternMaterializeStatus::InvalidBinding;
      synthBUsed = true;
    }

    const StepMask onsets = allOnsets(plan.bars[0].roles[roleIndex]);
    if (onsets != 0 && !ignored && !drumBound && !synthBound) {
      return PatternMaterializeStatus::UnboundRole;
    }
  }

  return PatternMaterializeStatus::Ok;
}

EventImportance importanceAt(const RoleRhythmPlan& role, StepMask bit) {
  if ((role.structural & bit) != 0) return EventImportance::Structural;
  if ((role.secondary & bit) != 0) return EventImportance::Secondary;
  return EventImportance::Ghost;
}

void accountImportance(EventImportance importance,
                       PatternMaterializationDiagnostics& diagnostics) {
  switch (importance) {
    case EventImportance::Structural:
      ++diagnostics.structuralEvents;
      break;
    case EventImportance::Secondary:
      ++diagnostics.secondaryEvents;
      break;
    case EventImportance::Ghost:
      ++diagnostics.ghostEvents;
      break;
    default:
      break;
  }
}

void materializeDrumRole(const RoleRhythmPlan& role,
                         int8_t drumVoice,
                         DrumPatternSet& drums,
                         PatternMaterializationDiagnostics& diagnostics) {
  DrumPattern& pattern = drums.voices[drumVoice];
  const StepMask onsets = allOnsets(role);
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsets & bit) == 0) continue;

    const EventImportance importance = importanceAt(role, bit);
    DrumStep& target = pattern.steps[step];
    target.hit = true;
    target.accent = (role.accents & bit) != 0;
    target.velocity = velocityFor(importance);
    target.timing = 0;
    target.fx = DRUM_FX_NONE;
    target.fxParam = 0;
    target.probability = 100;

    accountImportance(importance, diagnostics);
    if (target.accent) ++diagnostics.accentEvents;
  }
}

void materializeSynthRole(const RoleRhythmPlan& role,
                          const SynthRhythmBinding& binding,
                          SynthPattern& pattern,
                          PatternMaterializationDiagnostics& diagnostics) {
  const StepMask onsets = allOnsets(role);
  const StepMask gateIntent = static_cast<StepMask>(
      (role.shortGate | role.heldGate | role.tieGate) & onsets);
  diagnostics.deferredGateEvents = static_cast<uint16_t>(
      diagnostics.deferredGateEvents + popcount16(gateIntent));

  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsets & bit) == 0) continue;

    const EventImportance importance = importanceAt(role, bit);
    SynthStep& target = pattern.steps[step];
    target.note = binding.note;
    target.slide = false;
    target.accent = (role.accents & bit) != 0;
    target.ghost = importance == EventImportance::Ghost;
    target.velocity = velocityFor(importance);
    target.timing = 0;
    target.fx = 0;
    target.fxParam = 0;
    target.probability = 100;

    accountImportance(importance, diagnostics);
    if (target.accent) ++diagnostics.accentEvents;
  }
}

}  // namespace

PatternMaterializerBinding standardDrumPatternBinding(
    RhythmRoleMask ignoredRoles) {
  PatternMaterializerBinding binding{};
  binding.ignoredRoles = ignoredRoles;
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::Kick)] =
      static_cast<int8_t>(KICK);
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::Backbeat)] =
      static_cast<int8_t>(SNARE);
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::ClosedHat)] =
      static_cast<int8_t>(CLOSED_HAT);
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::OpenHat)] =
      static_cast<int8_t>(OPEN_HAT);
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::Percussion)] =
      static_cast<int8_t>(RIM);
  return binding;
}

PatternMaterializeStatus materializeRhythmPattern(
    const RhythmPhrasePlan& plan,
    const PatternMaterializerBinding& binding,
    MaterializedPatterns& destination,
    PatternMaterializationDiagnostics* diagnostics) {
  if (!validPlanShape(plan)) return PatternMaterializeStatus::InvalidPlan;

  const PatternMaterializeStatus bindingStatus = validateBinding(plan, binding);
  if (bindingStatus != PatternMaterializeStatus::Ok) return bindingStatus;

  MaterializedPatterns next{};
  PatternMaterializationDiagnostics nextDiagnostics{};
  const RhythmBarPlan& bar = plan.bars[0];

  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const RhythmRole role = static_cast<RhythmRole>(roleIndex);
    const RhythmRoleMask roleBit = rhythmRoleBit(role);
    const RoleRhythmPlan& rolePlan = bar.roles[roleIndex];
    const StepMask onsets = allOnsets(rolePlan);
    if (onsets == 0) continue;

    if ((binding.ignoredRoles & roleBit) != 0) {
      nextDiagnostics.ignoredRoles = static_cast<RhythmRoleMask>(
          nextDiagnostics.ignoredRoles | roleBit);
      nextDiagnostics.ignoredEvents = static_cast<uint16_t>(
          nextDiagnostics.ignoredEvents + popcount16(onsets));
      continue;
    }

    const int8_t drumVoice = binding.drumVoiceByRole[roleIndex];
    const SynthRhythmBinding& synthBinding = binding.synthByRole[roleIndex];
    if (drumVoice != kNoDrumVoice) {
      materializeDrumRole(rolePlan, drumVoice, next.drums,
                          nextDiagnostics);
    } else if (synthBinding.target == SynthPatternTarget::SynthA) {
      materializeSynthRole(rolePlan, synthBinding, next.synthA,
                           nextDiagnostics);
    } else {
      materializeSynthRole(rolePlan, synthBinding, next.synthB,
                           nextDiagnostics);
    }
    nextDiagnostics.emittedRoles = static_cast<RhythmRoleMask>(
        nextDiagnostics.emittedRoles | roleBit);
  }

  destination = next;
  if (diagnostics != nullptr) *diagnostics = nextDiagnostics;
  return PatternMaterializeStatus::Ok;
}

}  // namespace GroovePuterRhythm
