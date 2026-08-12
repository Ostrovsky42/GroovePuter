#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/roles/chord_rhythm_retrigger.h"

using namespace GroovePuterRhythm;

namespace {

bool hasStep(StepMask mask, uint8_t step) {
  return (mask & stepBit(step)) != 0;
}

StepMask rangeMask(uint8_t first, uint8_t lastInclusive) {
  StepMask result = 0;
  for (uint8_t step = first; step <= lastInclusive; ++step)
    result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

void testRetriggerDoesNotAdvanceSourceOrdinal() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.sameChordRetriggers = static_cast<StepMask>(stepBit(4) | stepBit(12));
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::Ok);
  assert(plan.sourceAdvanceCount == 2);
  assert(plan.audibleOnsetCount == 4);
  assert(plan.sourceOrdinalByStep[0] == 0);
  assert(plan.sourceOrdinalByStep[4] == 0);
  assert(plan.sourceOrdinalByStep[8] == 1);
  assert(plan.sourceOrdinalByStep[12] == 1);
}

void testIncomingSourceMakesBarStartRetriggerExplicitlyValid() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAvailableAtStart = true;
  request.sameChordRetriggers = stepBit(0);
  request.sourceAdvanceOnsets = stepBit(8);
  request.sameChordRetriggers = static_cast<StepMask>(
      request.sameChordRetriggers | stepBit(12));

  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::Ok);
  assert(plan.sourceAdvanceCount == 1);
  assert(plan.audibleOnsetCount == 3);
  assert(plan.sourceOrdinalByStep[0] == kIncomingChordRhythmSourceOrdinal);
  assert(plan.sourceOrdinalByStep[8] == 0);
  assert(plan.sourceOrdinalByStep[12] == 0);
}

void testRetriggerIsOnsetButDoesNotInferHold() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = stepBit(0);
  request.sameChordRetriggers = stepBit(4);
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::Ok);
  assert(hasStep(plan.audibleOnsets, 0));
  assert(hasStep(plan.audibleOnsets, 4));
  assert(plan.continuations == 0);
  assert(plan.releasePoints == 0);
}

void testExplicitGateTopologyIsPreservedAroundRetrigger() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.sameChordRetriggers = stepBit(4);
  request.continuations = static_cast<StepMask>(rangeMask(1, 3) |
                                                rangeMask(5, 6));
  request.releasePoints = stepBit(7);
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::Ok);
  assert(plan.continuations == request.continuations);
  assert(plan.releasePoints == request.releasePoints);
  assert(plan.sourceOrdinalByStep[0] == 0);
  assert(plan.sourceOrdinalByStep[4] == 0);
  assert(plan.sourceOrdinalByStep[8] == 1);
}

void testRetriggerAfterReleaseStillUsesCurrentSource() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = stepBit(0);
  request.releasePoints = stepBit(1);
  request.sameChordRetriggers = stepBit(4);
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::Ok);
  assert(plan.sourceAdvanceCount == 1);
  assert(plan.audibleOnsetCount == 2);
  assert(plan.sourceOrdinalByStep[4] == 0);
}

void testOrphanRetriggerFailsClosedWithoutIncomingSource() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = stepBit(4);
  request.sameChordRetriggers = stepBit(0);
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::OrphanRetrigger);
  assert(plan.sourceAdvanceOnsets == 0);
  assert(plan.sameChordRetriggers == 0);
  assert(plan.audibleOnsets == 0);
  assert(plan.continuations == 0);
  assert(plan.releasePoints == 0);
}

void testSemanticOverlapIsRejectedAsAmbiguous() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = stepBit(8);
  request.sameChordRetriggers = stepBit(8);
  assert(realizeChordRhythmRetriggers(request).status ==
         ChordRhythmRetriggerStatus::InvalidOverlap);

  request.sameChordRetriggers = 0;
  request.continuations = stepBit(8);
  assert(realizeChordRhythmRetriggers(request).status ==
         ChordRhythmRetriggerStatus::InvalidOverlap);
}

void testInvalidGateTopologyFailsClosed() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = stepBit(0);
  request.continuations = stepBit(2);
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::InvalidGateTopology);
  assert(plan.audibleOnsets == 0);
  assert(plan.continuations == 0);
  assert(plan.releasePoints == 0);
}

void testDenseRetriggerStillUsesOneSourceOrdinal() {
  ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = stepBit(0);
  for (uint8_t step = 1; step < 16; ++step)
    request.sameChordRetriggers = static_cast<StepMask>(
        request.sameChordRetriggers | stepBit(step));
  const ChordRhythmRetriggerPlan plan = realizeChordRhythmRetriggers(request);
  assert(plan.status == ChordRhythmRetriggerStatus::Ok);
  assert(plan.sourceAdvanceCount == 1);
  assert(plan.audibleOnsetCount == 16);
  assert(plan.continuations == 0);
  for (uint8_t step = 0; step < 16; ++step)
    assert(plan.sourceOrdinalByStep[step] == 0);
}

void testRetriggerChangesDoNotRewriteSourceAdvanceTopology() {
  ChordRhythmRetriggerRequest base{};
  base.sourceAdvanceOnsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  ChordRhythmRetriggerRequest variant = base;
  variant.sameChordRetriggers = static_cast<StepMask>(stepBit(2) | stepBit(6) |
                                                      stepBit(10) | stepBit(14));
  const ChordRhythmRetriggerPlan a = realizeChordRhythmRetriggers(base);
  const ChordRhythmRetriggerPlan b = realizeChordRhythmRetriggers(variant);
  assert(a.status == ChordRhythmRetriggerStatus::Ok);
  assert(b.status == ChordRhythmRetriggerStatus::Ok);
  assert(a.sourceAdvanceOnsets == b.sourceAdvanceOnsets);
  assert(a.sourceAdvanceCount == b.sourceAdvanceCount);
  assert(a.audibleOnsetCount == 2);
  assert(b.audibleOnsetCount == 6);
}

void testEmptyIsValidButEmpty() {
  const ChordRhythmRetriggerPlan plan =
      realizeChordRhythmRetriggers(ChordRhythmRetriggerRequest{});
  assert(plan.status == ChordRhythmRetriggerStatus::ValidButEmpty);
  assert(plan.audibleOnsets == 0);
  assert(plan.continuations == 0);
  assert(plan.releasePoints == 0);
  for (uint8_t step = 0; step < 16; ++step)
    assert(plan.sourceOrdinalByStep[step] == kNoChordRhythmSourceOrdinal);
}

}  // namespace

int main() {
  testRetriggerDoesNotAdvanceSourceOrdinal();
  testIncomingSourceMakesBarStartRetriggerExplicitlyValid();
  testRetriggerIsOnsetButDoesNotInferHold();
  testExplicitGateTopologyIsPreservedAroundRetrigger();
  testRetriggerAfterReleaseStillUsesCurrentSource();
  testOrphanRetriggerFailsClosedWithoutIncomingSource();
  testSemanticOverlapIsRejectedAsAmbiguous();
  testInvalidGateTopologyFailsClosed();
  testDenseRetriggerStillUsesOneSourceOrdinal();
  testRetriggerChangesDoNotRewriteSourceAdvanceTopology();
  testEmptyIsValidButEmpty();
  std::cout << "P3 same-chord retrigger host matrix: OK\n";
  return 0;
}
