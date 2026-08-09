#include "stage7a_session.h"

#include <cstdio>

#include "../../dsp/mini_drumvoices.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {
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
    case EventImportance::Structural: return 110;
    case EventImportance::Secondary: return 86;
    case EventImportance::Ghost: return 52;
    default: return 100;
  }
}

void applyMask(DrumPattern& pattern,
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

bool materializeDrumsOnly(const RhythmPhrasePlan& plan,
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
    applyMask(pattern, role.structural, EventImportance::Structural);
    applyMask(pattern, role.secondary, EventImportance::Secondary);
    applyMask(pattern, role.ghosts, EventImportance::Ghost);
  }
  return true;
}

}  // namespace

bool Session::establishIdentity() {
  RhythmRealizationRequest request{};
  request.catalog = &catalog();
  request.archetypeId = currentDefinition().archetypeId;
  request.phraseBars = 1;
  request.level = RealizationLevel::P1Canonical;
  request.generation.projectSeed = seed_;
  request.generation.phraseOrdinal = 0;

  const RhythmRealizationResult p1 = realizeRhythmPhrase(request);
  lastStatus_ = p1.status;
  if (p1.status == RealizationStatus::InvalidConstraintSet) {
    identityValid_ = false;
    return false;
  }

  identity_ = p1.identity;
  identityValid_ = true;
  return true;
}

bool Session::renderScratch(DrumPatternSet& drums,
                            SynthPattern& synthA,
                            SynthPattern& synthB) {
  if (!identityValid_ && !establishIdentity()) return false;

  RhythmRealizationRequest request{};
  request.catalog = &catalog();
  request.archetypeId = currentDefinition().archetypeId;
  request.phraseBars = 1;
  request.level = level_;
  request.generation.projectSeed = seed_;
  request.generation.phraseOrdinal = 0;
  request.reuseIdentity = &identity_;

  const RhythmRealizationResult result = realizeRhythmPhrase(request);
  lastStatus_ = result.status;
  if (result.status == RealizationStatus::InvalidConstraintSet) return false;

  return materializeDrumsOnly(result.plan, drums, synthA, synthB);
}

bool Session::commitCurrent(DrumPatternSet& drums,
                            SynthPattern& synthA,
                            SynthPattern& synthB) {
  if (!active_) return false;
  DrumPatternSet nextDrums{};
  SynthPattern nextA{};
  SynthPattern nextB{};
  if (!renderScratch(nextDrums, nextA, nextB)) return false;
  drums = nextDrums;
  synthA = nextA;
  synthB = nextB;
  return true;
}

bool Session::activate(DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB) {
  if (active_) return commitCurrent(drums, synthA, synthB);

  DrumPatternSet nextDrums{};
  SynthPattern nextA{};
  SynthPattern nextB{};
  if (!renderScratch(nextDrums, nextA, nextB)) return false;

  backupDrums_ = drums;
  backupA_ = synthA;
  backupB_ = synthB;
  drums = nextDrums;
  synthA = nextA;
  synthB = nextB;
  active_ = true;
  return true;
}

void Session::deactivate(DrumPatternSet& drums,
                         SynthPattern& synthA,
                         SynthPattern& synthB) {
  if (!active_) return;
  drums = backupDrums_;
  synthA = backupA_;
  synthB = backupB_;
  active_ = false;
}

bool Session::selectCandidate(uint8_t index,
                              DrumPatternSet& drums,
                              SynthPattern& synthA,
                              SynthPattern& synthB) {
  if (!active_ || index >= definitionCount()) return false;
  if (candidateIndex_ != index) {
    candidateIndex_ = index;
    identityValid_ = false;
    lastStatus_ = RealizationStatus::InvalidConstraintSet;
  }
  return commitCurrent(drums, synthA, synthB);
}

bool Session::shiftSeed(int delta,
                        DrumPatternSet& drums,
                        SynthPattern& synthA,
                        SynthPattern& synthB) {
  if (!active_ || delta == 0) return false;
  uint32_t next = seed_;
  if (delta < 0) {
    const uint32_t amount = static_cast<uint32_t>(-delta);
    next = amount >= seed_ ? 1u : seed_ - amount;
  } else {
    const uint32_t amount = static_cast<uint32_t>(delta);
    next = UINT32_MAX - seed_ < amount ? UINT32_MAX : seed_ + amount;
  }
  if (next != seed_) {
    seed_ = next;
    identityValid_ = false;
    lastStatus_ = RealizationStatus::InvalidConstraintSet;
  }
  return commitCurrent(drums, synthA, synthB);
}

bool Session::cycleLevel(DrumPatternSet& drums,
                         SynthPattern& synthA,
                         SynthPattern& synthB) {
  if (!active_) return false;
  uint8_t next = static_cast<uint8_t>(level_) + 1u;
  if (next >= static_cast<uint8_t>(RealizationLevel::Count)) next = 0;
  level_ = static_cast<RealizationLevel>(next);
  return commitCurrent(drums, synthA, synthB);
}

bool Session::rerender(DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB) {
  return commitCurrent(drums, synthA, synthB);
}

void Session::formatStatus(char* out, size_t capacity) const {
  if (!out || capacity == 0) return;
  const Definition& def = currentDefinition();
  std::snprintf(out,
                capacity,
                "S7A %s S%lu %s %s",
                def.name,
                static_cast<unsigned long>(seed_),
                levelName(level_),
                evidenceName(def.evidence));
}

const char* levelName(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical: return "P1";
    case RealizationLevel::P2Variation: return "P2";
    case RealizationLevel::P3Transformation: return "P3";
    default: return "P?";
  }
}

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
