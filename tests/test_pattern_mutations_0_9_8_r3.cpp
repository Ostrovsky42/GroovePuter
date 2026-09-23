#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/state/synth_pattern_edit.h"

namespace {
using namespace GroovePuterUndo::PatternEdit;

SynthPattern makeSequence() {
  SynthPattern pattern{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = static_cast<int8_t>(24 + i);
  }
  return pattern;
}

void testFullEquality() {
  SynthPattern a{};
  SynthPattern b = a;
  assert(samePattern(a, b));

  b.steps[3].timing = -7;
  assert(!samePattern(a, b));
  b = a;
  b.steps[3].velocity = 99;
  assert(!samePattern(a, b));
  b = a;
  b.steps[3].probability = 42;
  assert(!samePattern(a, b));
}

void testClearMatchesExistingMiniAcidSemantics() {
  SynthPattern pattern{};
  SynthStep& step = pattern.steps[4];
  step.note = 60;
  step.slide = true;
  step.accent = true;
  step.ghost = true;
  step.velocity = 77;
  step.timing = -5;
  step.fx = static_cast<uint8_t>(StepFx::Reverse);
  step.fxParam = 9;
  step.probability = 55;

  clearStep(pattern, 4);
  assert(step.note == -1);
  assert(!step.slide);
  assert(!step.accent);
  assert(!step.ghost);
  assert(step.velocity == 77);
  assert(step.timing == -5);
  assert(step.fx == 0);
  assert(step.fxParam == 0);
  assert(step.probability == 100);
}

void testNoteStateMachineAndNoOps() {
  SynthPattern pattern{};

  pattern.steps[0].note = -2;
  adjustNote(pattern, 0, -1);
  assert(pattern.steps[0].note == -2);
  adjustNote(pattern, 0, 1);
  assert(pattern.steps[0].note == -1);
  adjustNote(pattern, 0, 1);
  assert(pattern.steps[0].note == kMin303Note);
  adjustNote(pattern, 0, -1);
  assert(pattern.steps[0].note == -1);
  adjustNote(pattern, 0, -1);
  assert(pattern.steps[0].note == -2);

  pattern.steps[1].note = 70;
  adjustNote(pattern, 1, 5);
  assert(pattern.steps[1].note == kMax303Note);
  const SynthPattern saturated = pattern;
  adjustNote(pattern, 1, 1);
  assert(samePattern(pattern, saturated));

  pattern.steps[2].note = 60;
  adjustOctave(pattern, 2, 1);
  assert(pattern.steps[2].note == kMax303Note);
}

void testArticulationAndFx() {
  SynthPattern pattern{};
  toggleAccent(pattern, 2);
  toggleSlide(pattern, 2);
  assert(pattern.steps[2].accent);
  assert(pattern.steps[2].slide);
  setAccent(pattern, 2, false);
  setSlide(pattern, 2, false);
  assert(!pattern.steps[2].accent);
  assert(!pattern.steps[2].slide);

  cycleFx(pattern, 2);
  assert(pattern.steps[2].fx == static_cast<uint8_t>(StepFx::Retrig));
  assert(pattern.steps[2].fxParam == kDefaultRetrigCount);
  adjustFxParam(pattern, 2, 1);
  assert(pattern.steps[2].fxParam == kDefaultRetrigCount + 1);
  pattern.steps[2].fxParam = kMaxRetrigCount;
  const SynthPattern high = pattern;
  adjustFxParam(pattern, 2, 1);
  assert(samePattern(pattern, high));
  pattern.steps[2].fxParam = 1;
  const SynthPattern low = pattern;
  adjustFxParam(pattern, 2, -1);
  assert(samePattern(pattern, low));

  cycleFx(pattern, 2);
  assert(pattern.steps[2].fx == static_cast<uint8_t>(StepFx::None));
  assert(pattern.steps[2].fxParam == 0);

  // Legacy persisted Reverse is not a supported synth effect. Pressing F
  // converts it into the audible Retrig contract instead of exposing RV again.
  pattern.steps[2].fx = static_cast<uint8_t>(StepFx::Reverse);
  pattern.steps[2].fxParam = 0;
  cycleFx(pattern, 2);
  assert(pattern.steps[2].fx == static_cast<uint8_t>(StepFx::Retrig));
  assert(pattern.steps[2].fxParam == kDefaultRetrigCount);
}

void testRotation() {
  const SynthPattern original = makeSequence();
  SynthPattern pattern = original;

  rotate(pattern, 1);
  assert(pattern.steps[0].note == original.steps[SynthPattern::kSteps - 1].note);
  assert(pattern.steps[1].note == original.steps[0].note);

  rotate(pattern, -1);
  assert(samePattern(pattern, original));

  rotate(pattern, SynthPattern::kSteps);
  assert(samePattern(pattern, original));
}

}  // namespace

int main() {
  testFullEquality();
  testClearMatchesExistingMiniAcidSemantics();
  testNoteStateMachineAndNoOps();
  testArticulationAndFx();
  testRotation();
  std::puts("0.9.8 R3 prepared Pattern mutation semantics: PASS");
  return 0;
}
