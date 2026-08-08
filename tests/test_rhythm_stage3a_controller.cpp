#include <cassert>
#include <cstdint>

#include "src/generation/audition/rhythm_audition_controller.h"

using namespace GroovePuterRhythm;

namespace {

bool identityEqual(const PhraseRhythmIdentity& a,
                   const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId ||
      a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) {
    return false;
  }
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].steps != b.protectedSpaces[i].steps ||
        a.protectedSpaces[i].affectedRoles !=
            b.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  return true;
}

StepMask synthMask(const SynthPattern& pattern) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (pattern.steps[step].note >= 0) {
      mask = static_cast<StepMask>(mask | stepBit(step));
    }
  }
  return mask;
}

void testPLevelsReuseOneIdentity() {
  Audition::Controller controller;
  controller.selectDefinition(
      static_cast<uint8_t>(Audition::Archetype::ClassicTwoStep));
  controller.setSeed(17);

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};

  controller.setLevel(RealizationLevel::P1Canonical);
  assert(controller.render(drums, synthA, synthB));
  assert(controller.identityValid());
  const PhraseRhythmIdentity p1Identity = controller.identity();

  controller.setLevel(RealizationLevel::P2Variation);
  assert(controller.render(drums, synthA, synthB));
  assert(identityEqual(p1Identity, controller.identity()));

  controller.setLevel(RealizationLevel::P3Transformation);
  assert(controller.render(drums, synthA, synthB));
  assert(identityEqual(p1Identity, controller.identity()));
}

void testSeedAndArchetypeInvalidateIdentity() {
  Audition::Controller controller;
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};

  assert(controller.render(drums, synthA, synthB));
  assert(controller.identityValid());

  controller.shiftSeed(1);
  assert(controller.seed() == 2);
  assert(!controller.identityValid());
  assert(controller.render(drums, synthA, synthB));
  assert(controller.identityValid());
  assert(controller.identity().archetypeId ==
         controller.currentDefinition().archetypeId);

  controller.selectDefinition(
      static_cast<uint8_t>(Audition::Archetype::SparseSkank));
  assert(!controller.identityValid());
  assert(controller.render(drums, synthA, synthB));
  assert(controller.identity().archetypeId ==
         controller.currentDefinition().archetypeId);
}

void testBassToggleDoesNotRerollIdentity() {
  Audition::Controller controller;
  controller.selectDefinition(
      static_cast<uint8_t>(Audition::Archetype::RollingAcid));
  controller.setSeed(23);

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  assert(controller.render(drums, synthA, synthB));
  const PhraseRhythmIdentity identity = controller.identity();
  assert(synthMask(synthA) == 0);

  controller.toggleBass();
  assert(controller.bassEnabled());
  assert(controller.render(drums, synthA, synthB));
  assert(identityEqual(identity, controller.identity()));
  assert(synthMask(synthA) != 0);
  assert(synthMask(synthB) == 0);
}

void testSeedBoundsAndLevelCycle() {
  Audition::Controller controller;
  controller.setSeed(1);
  controller.shiftSeed(-100);
  assert(controller.seed() == 1);
  controller.setSeed(UINT32_MAX - 1u);
  controller.shiftSeed(10);
  assert(controller.seed() == UINT32_MAX);

  controller.setLevel(RealizationLevel::P1Canonical);
  controller.cycleLevel();
  assert(controller.level() == RealizationLevel::P2Variation);
  controller.cycleLevel();
  assert(controller.level() == RealizationLevel::P3Transformation);
  controller.cycleLevel();
  assert(controller.level() == RealizationLevel::P1Canonical);
}

}  // namespace

int main() {
  testPLevelsReuseOneIdentity();
  testSeedAndArchetypeInvalidateIdentity();
  testBassToggleDoesNotRerollIdentity();
  testSeedBoundsAndLevelCycle();
  return 0;
}
