#include "rhythm_audition_materializer.h"

namespace GroovePuterRhythm {
namespace Audition {
namespace {

struct DrumBinding {
  RhythmRole role;
  uint8_t voice;
};

constexpr DrumBinding kDrumBindings[] = {
    {RhythmRole::Kick, static_cast<uint8_t>(KICK)},
    {RhythmRole::Backbeat, static_cast<uint8_t>(SNARE)},
    {RhythmRole::ClosedHat, static_cast<uint8_t>(CLOSED_HAT)},
    {RhythmRole::OpenHat, static_cast<uint8_t>(OPEN_HAT)},
    {RhythmRole::Percussion, static_cast<uint8_t>(RIM)},
};

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

void materializeDrumMask(DrumPattern& pattern,
                         StepMask mask,
                         EventImportance importance) {
  const uint8_t velocity = velocityFor(importance);
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (!(mask & stepBit(step))) continue;
    DrumStep& target = pattern.steps[step];
    target.hit = true;
    target.accent = false;
    target.velocity = velocity;
    target.timing = 0;
    target.fx = DRUM_FX_NONE;
    target.fxParam = 0;
    target.probability = 100;
  }
}

void materializeSynthMask(SynthPattern& pattern,
                          StepMask mask,
                          int8_t note,
                          EventImportance importance) {
  const uint8_t velocity = velocityFor(importance);
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (!(mask & stepBit(step))) continue;
    SynthStep& target = pattern.steps[step];
    target.note = note;
    target.slide = false;
    target.accent = false;
    target.ghost = importance == EventImportance::Ghost;
    target.velocity = velocity;
    target.timing = 0;
    target.fx = 0;
    target.fxParam = 0;
    target.probability = 100;
  }
}

}  // namespace

bool materializeOneBar(const RhythmPhrasePlan& plan,
                       const MaterializeOptions& options,
                       DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB) {
  if (plan.barCount != 1 ||
      plan.trajectoryId != kNoTrajectoryId ||
      plan.intent != TransformationIntent::Auto ||
      plan.bars[0].function != BarFunction::Statement) {
    return false;
  }

  drums = DrumPatternSet{};
  synthA = SynthPattern{};
  synthB = SynthPattern{};
  drums.groove.swing = 0.0f;
  drums.groove.humanize = 0.0f;

  const RhythmBarPlan& bar = plan.bars[0];
  for (const DrumBinding& binding : kDrumBindings) {
    const RoleRhythmPlan& role =
        bar.roles[static_cast<uint8_t>(binding.role)];
    DrumPattern& pattern = drums.voices[binding.voice];
    materializeDrumMask(pattern, role.structural,
                        EventImportance::Structural);
    materializeDrumMask(pattern, role.secondary,
                        EventImportance::Secondary);
    materializeDrumMask(pattern, role.ghosts,
                        EventImportance::Ghost);
  }

  if (options.bassEnabled) {
    const RoleRhythmPlan& bass =
        bar.roles[static_cast<uint8_t>(RhythmRole::BassRhythm)];
    materializeSynthMask(synthA, bass.structural, options.bassNote,
                         EventImportance::Structural);
    materializeSynthMask(synthA, bass.secondary, options.bassNote,
                         EventImportance::Secondary);
    materializeSynthMask(synthA, bass.ghosts, options.bassNote,
                         EventImportance::Ghost);
  }

  return true;
}

}  // namespace Audition
}  // namespace GroovePuterRhythm
