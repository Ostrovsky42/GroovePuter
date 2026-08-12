#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/roles/chord_rhythm_timeline.h"

using namespace GroovePuterRhythm;

namespace {

ChordRhythmTimelineMask maskForSteps(const uint8_t* steps, uint8_t count) {
  ChordRhythmTimelineMask result = 0;
  for (uint8_t index = 0; index < count; ++index)
    result |= chordRhythmTimelineStepBit(steps[index]);
  return result;
}

ChordRhythmTimelineMask rangeMask(uint8_t first, uint8_t lastInclusive) {
  ChordRhythmTimelineMask result = 0;
  for (uint8_t step = first; step <= lastInclusive; ++step)
    result |= chordRhythmTimelineStepBit(step);
  return result;
}

bool hasStep(ChordRhythmTimelineMask mask, uint8_t step) {
  return (mask & chordRhythmTimelineStepBit(step)) != 0;
}

void testHeldPadPreservesExplicitRelease() {
  ChordRhythmTimelineRequest request{};
  request.barCount = 1;
  request.onsets = chordRhythmTimelineStepBit(0);
  request.continuations = rangeMask(1, 11);
  request.releasePoints = chordRhythmTimelineStepBit(12);

  const ChordRhythmTimelinePlan plan = realizeChordRhythmTimeline(request);
  assert(plan.status == ChordRhythmTimelineStatus::Ok);
  assert(plan.onsets == request.onsets);
  assert(plan.continuations == request.continuations);
  assert(plan.releasePoints == request.releasePoints);
  assert(plan.onsetCount == 1);
  assert(!hasStep(plan.continuations, 13));
}

void testStabDoesNotBecomeInferredHold() {
  const uint8_t onsetSteps[] = {2, 6, 10, 14};
  ChordRhythmTimelineRequest request{};
  request.barCount = 1;
  request.onsets = maskForSteps(onsetSteps, 4);

  const ChordRhythmTimelinePlan plan = realizeChordRhythmTimeline(request);
  assert(plan.status == ChordRhythmTimelineStatus::Ok);
  assert(plan.onsetCount == 4);
  assert(plan.continuations == 0);
  assert(plan.releasePoints == 0);
}

void testDeclaredContinuationCrossesBarBoundary() {
  ChordRhythmTimelineRequest request{};
  request.barCount = 2;
  request.onsets = chordRhythmTimelineStepBit(12);
  request.continuations = rangeMask(13, 19);
  request.releasePoints = chordRhythmTimelineStepBit(20);

  const ChordRhythmTimelinePlan plan = realizeChordRhythmTimeline(request);
  assert(plan.status == ChordRhythmTimelineStatus::Ok);
  assert(hasStep(plan.continuations, 15));
  assert(hasStep(plan.continuations, 16));
  assert(hasStep(plan.continuations, 19));
  assert(hasStep(plan.releasePoints, 20));
}

void testFourBarsMayContainMoreThanEightRhythmOnsets() {
  ChordRhythmTimelineRequest request{};
  request.barCount = 4;
  for (uint8_t step = 0; step < 64; step = static_cast<uint8_t>(step + 4))
    request.onsets |= chordRhythmTimelineStepBit(step);

  const ChordRhythmTimelinePlan plan = realizeChordRhythmTimeline(request);
  assert(plan.status == ChordRhythmTimelineStatus::Ok);
  assert(plan.stepCount == 64);
  assert(plan.onsetCount == 16);
  assert(plan.onsetCount > 8);
  assert(plan.continuations == 0);
  assert(plan.releasePoints == 0);
}

void testInvalidBoundsAndOverlapFailClosed() {
  ChordRhythmTimelineRequest request{};
  request.barCount = 5;
  request.onsets = chordRhythmTimelineStepBit(0);
  assert(realizeChordRhythmTimeline(request).status ==
         ChordRhythmTimelineStatus::InvalidRequest);

  request.barCount = 1;
  request.onsets = chordRhythmTimelineStepBit(16);
  assert(realizeChordRhythmTimeline(request).status ==
         ChordRhythmTimelineStatus::InvalidRequest);

  request.onsets = chordRhythmTimelineStepBit(4);
  request.continuations = chordRhythmTimelineStepBit(4);
  const ChordRhythmTimelinePlan overlap = realizeChordRhythmTimeline(request);
  assert(overlap.status == ChordRhythmTimelineStatus::InvalidRequest);
  assert(overlap.onsets == 0 && overlap.continuations == 0 &&
         overlap.releasePoints == 0);
}

void testOrphanGateEventsAreRejected() {
  ChordRhythmTimelineRequest request{};
  request.barCount = 1;
  request.continuations = chordRhythmTimelineStepBit(0);
  assert(realizeChordRhythmTimeline(request).status ==
         ChordRhythmTimelineStatus::InvalidTopology);

  request.continuations = 0;
  request.releasePoints = chordRhythmTimelineStepBit(0);
  assert(realizeChordRhythmTimeline(request).status ==
         ChordRhythmTimelineStatus::InvalidTopology);

  request.onsets = chordRhythmTimelineStepBit(0);
  request.releasePoints = 0;
  request.continuations = chordRhythmTimelineStepBit(2);  // gap at step 1
  assert(realizeChordRhythmTimeline(request).status ==
         ChordRhythmTimelineStatus::InvalidTopology);
}

void testEmptyTimelineIsExplicitlyValidButEmpty() {
  ChordRhythmTimelineRequest request{};
  request.barCount = 3;
  const ChordRhythmTimelinePlan plan = realizeChordRhythmTimeline(request);
  assert(plan.status == ChordRhythmTimelineStatus::ValidButEmpty);
  assert(plan.barCount == 3);
  assert(plan.stepCount == 48);
  assert(plan.onsetCount == 0);
  assert(plan.onsets == 0 && plan.continuations == 0 &&
         plan.releasePoints == 0);
}

void testBarConversionMatchesStepMaskConvention() {
  const StepMask legacy = static_cast<StepMask>(stepBit(0) | stepBit(8) |
                                                stepBit(15));
  const ChordRhythmTimelineMask bar0 = chordRhythmTimelineFromBar(legacy, 0);
  const ChordRhythmTimelineMask bar2 = chordRhythmTimelineFromBar(legacy, 2);
  assert(hasStep(bar0, 0));
  assert(hasStep(bar0, 8));
  assert(hasStep(bar0, 15));
  assert(hasStep(bar2, 32));
  assert(hasStep(bar2, 40));
  assert(hasStep(bar2, 47));
  assert(chordRhythmTimelineFromBar(legacy, 4) == 0);
}

}  // namespace

int main() {
  testHeldPadPreservesExplicitRelease();
  testStabDoesNotBecomeInferredHold();
  testDeclaredContinuationCrossesBarBoundary();
  testFourBarsMayContainMoreThanEightRhythmOnsets();
  testInvalidBoundsAndOverlapFailClosed();
  testOrphanGateEventsAreRejected();
  testEmptyTimelineIsExplicitlyValidButEmpty();
  testBarConversionMatchesStepMaskConvention();
  std::cout << "P2 bounded multi-bar ChordRhythm host matrix: OK\n";
  return 0;
}
