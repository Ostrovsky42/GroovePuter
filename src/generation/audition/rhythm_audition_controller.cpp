#include "rhythm_audition_controller.h"

namespace GroovePuterRhythm {
namespace Audition {
namespace {

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

}  // namespace

const Definition& Controller::currentDefinition() const {
  return definition(definitionIndex_);
}

void Controller::selectDefinition(uint8_t index) {
  if (index >= definitionCount()) index = 0;
  if (definitionIndex_ == index) return;
  definitionIndex_ = index;
  identityValid_ = false;
  lastStatus_ = RealizationStatus::InvalidConstraintSet;
}

void Controller::setSeed(uint32_t seed) {
  if (seed == 0) seed = 1;
  if (seed_ == seed) return;
  seed_ = seed;
  identityValid_ = false;
  lastStatus_ = RealizationStatus::InvalidConstraintSet;
}

void Controller::shiftSeed(int delta) {
  if (delta == 0) return;
  if (delta < 0) {
    const uint32_t amount = static_cast<uint32_t>(-delta);
    setSeed(amount >= seed_ ? 1u : seed_ - amount);
    return;
  }
  const uint32_t amount = static_cast<uint32_t>(delta);
  if (UINT32_MAX - seed_ < amount) {
    setSeed(UINT32_MAX);
  } else {
    setSeed(seed_ + amount);
  }
}

void Controller::cycleLevel() {
  uint8_t next = static_cast<uint8_t>(level_) + 1u;
  if (next >= static_cast<uint8_t>(RealizationLevel::Count)) next = 0;
  level_ = static_cast<RealizationLevel>(next);
}

void Controller::setLevel(RealizationLevel level) {
  level_ = validLevel(level) ? level : RealizationLevel::P1Canonical;
}

bool Controller::establishIdentity() {
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

bool Controller::render(DrumPatternSet& drums,
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

  MaterializeOptions options{};
  options.bassEnabled = bassEnabled_;
  options.bassNote = 36;

  DrumPatternSet nextDrums{};
  SynthPattern nextA{};
  SynthPattern nextB{};
  if (!materializeOneBar(result.plan, options, nextDrums, nextA, nextB)) {
    return false;
  }

  drums = nextDrums;
  synthA = nextA;
  synthB = nextB;
  return true;
}

const char* levelName(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical:
      return "P1";
    case RealizationLevel::P2Variation:
      return "P2";
    case RealizationLevel::P3Transformation:
      return "P3";
    default:
      return "P?";
  }
}

}  // namespace Audition
}  // namespace GroovePuterRhythm
