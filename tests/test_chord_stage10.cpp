#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/roles/chord_rhythm.h"
#include "src/generation/roles/semantic_pattern_projector.h"

using namespace GroovePuterRhythm;

namespace {

ChordRhythmRequest requestFor(ChordRhythmId id) {
  ChordRhythmRequest request{};
  request.requestedId = id;
  request.family = RhythmFamily::DubPulse;
  request.archetypeId = 412;
  request.bassOnsets = static_cast<StepMask>(stepBit(0) | stepBit(9));
  request.generation.projectSeed = 0xC4010010u;
  request.generation.phraseOrdinal = 4;
  return request;
}

SynthPattern progression(int transposition) {
  SynthPattern pattern{};
  pattern.steps[0].note = static_cast<int8_t>(48 + transposition);
  pattern.steps[4].note = static_cast<int8_t>(53 + transposition);
  pattern.steps[8].note = static_cast<int8_t>(55 + transposition);
  pattern.steps[12].note = static_cast<int8_t>(46 + transposition);
  return pattern;
}

StepMask semanticOnsets(const SynthPattern& pattern,
                        const ChordRhythmPlan& plan) {
  StepMask result = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if ((plan.onsets & stepBit(step)) != 0 && pattern.steps[step].note >= 0) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

void testEveryIdentityAndDurations() {
  for (uint8_t value = static_cast<uint8_t>(ChordRhythmId::HeldPad);
       value < static_cast<uint8_t>(ChordRhythmId::Count); ++value) {
    ChordRhythmRequest request = requestFor(static_cast<ChordRhythmId>(value));
    request.bassOnsets = 0;
    if (request.requestedId == ChordRhythmId::SparseChordReply) {
      request.bassOnsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
    }
    const ChordRhythmResult result = realizeChordRhythm(request);
    assert(result.status == ChordRhythmStatus::Ok);
    assert(result.plan.id == static_cast<ChordRhythmId>(value));
    assert(std::strcmp(chordRhythmName(result.plan.id), "INVALID") != 0);
  }
  const ChordRhythmResult whole = realizeChordRhythm(
      [] { auto r = requestFor(ChordRhythmId::WholeBarHold);
           r.bassOnsets = 0; return r; }());
  assert(whole.plan.onsets == stepBit(0));
  assert(whole.plan.continuations == static_cast<StepMask>(~stepBit(0)));
  const ChordRhythmResult halves = realizeChordRhythm(
      [] { auto r = requestFor(ChordRhythmId::HalfBarChange);
           r.bassOnsets = 0; return r; }());
  assert(halves.plan.onsets == static_cast<StepMask>(stepBit(0) | stepBit(8)));
  assert((halves.plan.continuations & halves.plan.onsets) == 0);
}

void testHarmonyIsIndependentFromChordRhythm() {
  ChordRhythmRequest request = requestFor(ChordRhythmId::OffbeatStab);
  request.bassOnsets = 0;
  const ChordRhythmResult rhythm = realizeChordRhythm(request);
  SynthPattern first{};
  SynthPattern second{};
  assert(projectLegacyPitchPattern(progression(0), rhythm.plan.onsets,
                                   rhythm.plan.continuations, first) ==
         SemanticPatternProjectStatus::Ok);
  assert(projectLegacyPitchPattern(progression(7), rhythm.plan.onsets,
                                   rhythm.plan.continuations, second) ==
         SemanticPatternProjectStatus::Ok);
  assert(semanticOnsets(first, rhythm.plan) == rhythm.plan.onsets);
  assert(semanticOnsets(second, rhythm.plan) == rhythm.plan.onsets);
  assert(std::memcmp(&first, &second, sizeof(first)) != 0);
}

void testWholeBarHoldProjectionAndFeel() {
  ChordRhythmRequest request = requestFor(ChordRhythmId::WholeBarHold);
  request.bassOnsets = 0;
  const ChordRhythmResult rhythm = realizeChordRhythm(request);
  SynthPattern pattern{};
  assert(projectLegacyPitchPattern(progression(0), rhythm.plan.onsets,
                                   rhythm.plan.continuations, pattern) ==
         SemanticPatternProjectStatus::Ok);
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    assert(pattern.steps[step].note == 48);
    assert(pattern.steps[step].slide == (step != 0));
  }
  assert(applyFeelToSemanticPattern(
             RhythmRole::ChordRhythm, rhythm.plan.onsets,
             FeelProfileId::LaidBack, 100, request.generation, pattern) ==
         FeelInterpretStatus::Ok);
  assert(pattern.steps[0].timing > 0);
  for (uint8_t step = 1; step < SynthPattern::kSteps; ++step) {
    assert(pattern.steps[step].timing == 0);
  }
}

void testProtectedSpaceAndEmptyBarAreValid() {
  ChordRhythmRequest protectedRequest = requestFor(ChordRhythmId::OffbeatStab);
  protectedRequest.protectedSpace = static_cast<StepMask>(stepBit(2) | stepBit(10));
  protectedRequest.bassOnsets = stepBit(6);
  const ChordRhythmResult protectedResult = realizeChordRhythm(protectedRequest);
  assert(protectedResult.plan.onsets == stepBit(14));

  ChordRhythmRequest empty = requestFor(ChordRhythmId::DubChordSpace);
  empty.bassOnsets = 0;
  empty.allowEmptyBar = true;
  empty.barOrdinal = 1;
  const ChordRhythmResult result = realizeChordRhythm(empty);
  assert(result.status == ChordRhythmStatus::ValidButEmpty);
  assert(result.plan.onsets == 0 && result.plan.continuations == 0);
}

void testAutoDeterminism() {
  for (uint8_t family = 0;
       family < static_cast<uint8_t>(RhythmFamily::Count); ++family) {
    ChordRhythmRequest request = requestFor(ChordRhythmId::Auto);
    request.family = static_cast<RhythmFamily>(family);
    request.bassOnsets = 0;
    for (uint16_t ordinal = 0; ordinal < 64; ++ordinal) {
      request.generation.phraseOrdinal = ordinal;
      const ChordRhythmResult first = realizeChordRhythm(request);
      const ChordRhythmResult second = realizeChordRhythm(request);
      assert(first.status == second.status);
      assert(std::memcmp(&first.plan, &second.plan, sizeof(first.plan)) == 0);
      assert(first.plan.id != ChordRhythmId::Auto);
    }
  }
}

}  // namespace

int main() {
  testEveryIdentityAndDurations();
  testHarmonyIsIndependentFromChordRhythm();
  testWholeBarHoldProjectionAndFeel();
  testProtectedSpaceAndEmptyBarAreValid();
  testAutoDeterminism();
  return 0;
}
