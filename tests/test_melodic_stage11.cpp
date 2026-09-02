#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/roles/melodic_motif.h"
#include "src/generation/roles/semantic_pattern_projector.h"

using namespace GroovePuterRhythm;

namespace {

MelodicMotifRequest requestFor(MelodicRhythmId rhythm,
                               MotifShapeId shape = MotifShapeId::SourceOrder) {
  MelodicMotifRequest request{};
  request.requestedRhythm = rhythm;
  request.requestedShape = shape;
  request.family = RhythmFamily::HipHopBackbeat;
  request.archetypeId = 416;
  request.generation.projectSeed = 0xA11CE011u;
  request.generation.phraseOrdinal = 9;
  return request;
}

SynthPattern pitchPhrase() {
  SynthPattern pattern{};
  pattern.steps[0].note = 60;
  pattern.steps[4].note = 64;
  pattern.steps[8].note = 67;
  pattern.steps[12].note = 71;
  return pattern;
}

uint8_t countOnsets(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    mask = static_cast<StepMask>(mask & static_cast<StepMask>(mask - 1u));
    ++count;
  }
  return count;
}

bool equalPlan(const MelodicMotifPlan& a, const MelodicMotifPlan& b) {
  if (a.rhythmId != b.rhythmId || a.onsets != b.onsets ||
      a.continuations != b.continuations ||
      a.motif.shape != b.motif.shape ||
      a.motif.sourceOrderCount != b.motif.sourceOrderCount) {
    return false;
  }
  for (uint8_t index = 0; index < a.motif.sourceOrderCount; ++index) {
    if (a.motif.sourceOrder[index] != b.motif.sourceOrder[index]) return false;
  }
  return true;
}

void testEveryRhythmIdentity() {
  for (uint8_t value = static_cast<uint8_t>(MelodicRhythmId::SparseCall);
       value < static_cast<uint8_t>(MelodicRhythmId::Count); ++value) {
    const MelodicMotifResult result = realizeMelodicMotif(
        requestFor(static_cast<MelodicRhythmId>(value)));
    assert(result.status == MelodicMotifStatus::Ok);
    assert(result.plan.rhythmId == static_cast<MelodicRhythmId>(value));
    assert(countOnsets(result.plan.onsets) <= 4);
    assert(std::strcmp(melodicRhythmName(result.plan.rhythmId), "INVALID") != 0);
  }
}

void testSilenceIsFirstClass() {
  MelodicMotifRequest rest = requestFor(MelodicRhythmId::RestHeavy);
  rest.allowEmptyBar = true;
  rest.barOrdinal = 1;
  const MelodicMotifResult empty = realizeMelodicMotif(rest);
  assert(empty.status == MelodicMotifStatus::ValidButEmpty);
  assert(countOnsets(empty.plan.onsets) == 0);

  rest.barOrdinal = 4;
  const MelodicMotifResult one = realizeMelodicMotif(rest);
  assert(one.status == MelodicMotifStatus::Ok);
  assert(countOnsets(one.plan.onsets) == 1);

  const MelodicMotifResult two = realizeMelodicMotif(
      requestFor(MelodicRhythmId::TwoNoteHook));
  const MelodicMotifResult three = realizeMelodicMotif(
      requestFor(MelodicRhythmId::PickupPhrase));
  assert(countOnsets(two.plan.onsets) == 2);
  assert(countOnsets(three.plan.onsets) == 3);
}

void testMotifChangesOrderNotTopologyOrPitchSet() {
  const MelodicMotifResult sourceOrder = realizeMelodicMotif(
      requestFor(MelodicRhythmId::RepeatedCell,
                 MotifShapeId::SourceOrder));
  const MelodicMotifResult cell = realizeMelodicMotif(
      requestFor(MelodicRhythmId::RepeatedCell,
                 MotifShapeId::TwoNoteCell));
  assert(sourceOrder.plan.onsets == cell.plan.onsets);

  SynthPattern first{};
  SynthPattern second{};
  assert(projectLegacyPitchPatternWithOrder(
             pitchPhrase(), sourceOrder.plan.onsets,
             sourceOrder.plan.continuations,
             sourceOrder.plan.motif.sourceOrder,
             sourceOrder.plan.motif.sourceOrderCount, first) ==
         SemanticPatternProjectStatus::Ok);
  assert(projectLegacyPitchPatternWithOrder(
             pitchPhrase(), cell.plan.onsets, cell.plan.continuations,
             cell.plan.motif.sourceOrder,
             cell.plan.motif.sourceOrderCount, second) ==
         SemanticPatternProjectStatus::Ok);
  assert(first.steps[0].note == 60 && first.steps[4].note == 64 &&
         first.steps[8].note == 67 && first.steps[12].note == 71);
  assert(second.steps[0].note == 60 && second.steps[4].note == 64 &&
         second.steps[8].note == 60 && second.steps[12].note == 64);
}

void testLongToneAndFeel() {
  const MelodicMotifRequest request = requestFor(MelodicRhythmId::LongTone);
  const MelodicMotifResult result = realizeMelodicMotif(request);
  assert(result.plan.continuations != 0);
  SynthPattern pattern{};
  assert(projectLegacyPitchPatternWithOrder(
             pitchPhrase(), result.plan.onsets, result.plan.continuations,
             result.plan.motif.sourceOrder,
             result.plan.motif.sourceOrderCount, pattern) ==
         SemanticPatternProjectStatus::Ok);
  assert(applyFeelToSemanticPattern(
             RhythmRole::MelodicRhythm, result.plan.onsets,
             FeelProfileId::PushPullControlled, 100,
             request.generation, pattern) == FeelInterpretStatus::Ok);
  assert(pattern.steps[3].timing != 0);
  for (uint8_t step = 4; step <= 11; ++step) {
    assert(pattern.steps[step].note == 60 && pattern.steps[step].slide);
  }
}

void testProtectedRolesAndDeterminism() {
  MelodicMotifRequest blocked = requestFor(MelodicRhythmId::SyncopatedMotif);
  blocked.bassOnsets = stepBit(1);
  blocked.chordOnsets = stepBit(5);
  blocked.protectedSpace = stepBit(10);
  const MelodicMotifResult result = realizeMelodicMotif(blocked);
  assert(result.plan.onsets == stepBit(14));

  for (uint8_t family = 0;
       family < static_cast<uint8_t>(RhythmFamily::Count); ++family) {
    MelodicMotifRequest automatic = requestFor(
        MelodicRhythmId::Auto, MotifShapeId::Auto);
    automatic.family = static_cast<RhythmFamily>(family);
    for (uint16_t ordinal = 0; ordinal < 64; ++ordinal) {
      automatic.generation.phraseOrdinal = ordinal;
      const MelodicMotifResult first = realizeMelodicMotif(automatic);
      const MelodicMotifResult second = realizeMelodicMotif(automatic);
      assert(first.status == second.status);
      assert(equalPlan(first.plan, second.plan));
      assert(first.plan.rhythmId != MelodicRhythmId::Auto);
      assert(first.plan.motif.shape != MotifShapeId::Auto);
    }
  }
}

}  // namespace

int main() {
  testEveryRhythmIdentity();
  testSilenceIsFirstClass();
  testMotifChangesOrderNotTopologyOrPitchSet();
  testLongToneAndFeel();
  testProtectedRolesAndDeterminism();
  return 0;
}
