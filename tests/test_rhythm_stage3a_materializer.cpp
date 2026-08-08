#include <cassert>
#include <cstdint>

#include "src/generation/audition/rhythm_audition_catalog.h"
#include "src/generation/audition/rhythm_audition_materializer.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

StepMask drumMask(const DrumPattern& pattern) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (pattern.steps[step].hit) {
      mask = static_cast<StepMask>(mask | stepBit(step));
    }
  }
  return mask;
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

StepMask roleOnsets(const RhythmPhrasePlan& plan, RhythmRole role) {
  const RoleRhythmPlan& value =
      plan.bars[0].roles[static_cast<uint8_t>(role)];
  return static_cast<StepMask>(
      value.structural | value.secondary | value.ghosts);
}

RhythmRealizationResult realize(const Audition::Definition& definition,
                                uint32_t seed,
                                RealizationLevel level) {
  RhythmRealizationRequest request{};
  request.catalog = &Audition::catalog();
  request.archetypeId = definition.archetypeId;
  request.phraseBars = 1;
  request.level = level;
  request.generation.projectSeed = seed;
  return realizeRhythmPhrase(request);
}

void assertDrumBinding(const RhythmPhrasePlan& plan,
                       const DrumPatternSet& drums) {
  assert(drumMask(drums.voices[KICK]) ==
         roleOnsets(plan, RhythmRole::Kick));
  assert(drumMask(drums.voices[SNARE]) ==
         roleOnsets(plan, RhythmRole::Backbeat));
  assert(drumMask(drums.voices[CLOSED_HAT]) ==
         roleOnsets(plan, RhythmRole::ClosedHat));
  assert(drumMask(drums.voices[OPEN_HAT]) ==
         roleOnsets(plan, RhythmRole::OpenHat));
  assert(drumMask(drums.voices[RIM]) ==
         roleOnsets(plan, RhythmRole::Percussion));

  // Stage 3A intentionally does not bind extra physical drum voices.
  assert(drumMask(drums.voices[MID_TOM]) == 0);
  assert(drumMask(drums.voices[HIGH_TOM]) == 0);
  assert(drumMask(drums.voices[CLAP]) == 0);
}

void testMaterializerPreservesTopology() {
  assert(validateRhythmCatalog(Audition::catalog()));

  for (uint8_t i = 0; i < Audition::definitionCount(); ++i) {
    const Audition::Definition& definition = Audition::definition(i);
    for (uint32_t seed = 1; seed <= 16; ++seed) {
      const RhythmRealizationResult result = realize(
          definition, seed, RealizationLevel::P3Transformation);
      assert(result.status != RealizationStatus::InvalidConstraintSet);

      DrumPatternSet drums{};
      SynthPattern synthA{};
      SynthPattern synthB{};
      Audition::MaterializeOptions options{};
      options.bassEnabled = false;
      assert(Audition::materializeOneBar(
          result.plan, options, drums, synthA, synthB));
      assertDrumBinding(result.plan, drums);
      assert(synthMask(synthA) == 0);
      assert(synthMask(synthB) == 0);

      options.bassEnabled = true;
      options.bassNote = 36;
      assert(Audition::materializeOneBar(
          result.plan, options, drums, synthA, synthB));
      assertDrumBinding(result.plan, drums);
      assert(synthMask(synthA) ==
             roleOnsets(result.plan, RhythmRole::BassRhythm));
      assert(synthMask(synthB) == 0);
      for (uint8_t step = 0; step < kStepsPerBar; ++step) {
        if (synthA.steps[step].note >= 0) {
          assert(synthA.steps[step].note == 36);
          assert(!synthA.steps[step].slide);
          assert(!synthA.steps[step].accent);
        }
      }
    }
  }
}

void testInvalidPlanDoesNotMutateOutputs() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  drums.voices[KICK].steps[0].hit = true;
  synthA.steps[0].note = 48;
  synthB.steps[1].note = 55;

  RhythmPhrasePlan invalid{};
  invalid.barCount = 2;
  invalid.bars[0].function = BarFunction::Statement;
  invalid.bars[1].function = BarFunction::Statement;

  const DrumPatternSet beforeDrums = drums;
  const SynthPattern beforeA = synthA;
  const SynthPattern beforeB = synthB;

  Audition::MaterializeOptions options{};
  assert(!Audition::materializeOneBar(
      invalid, options, drums, synthA, synthB));

  assert(drums.voices[KICK].steps[0].hit ==
         beforeDrums.voices[KICK].steps[0].hit);
  assert(synthA.steps[0].note == beforeA.steps[0].note);
  assert(synthB.steps[1].note == beforeB.steps[1].note);
}

}  // namespace

int main() {
  testMaterializerPreservesTopology();
  testInvalidPlanDoesNotMutateOutputs();
  return 0;
}
