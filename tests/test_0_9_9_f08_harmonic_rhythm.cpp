#include <cassert>
#include <cstdint>

#include "src/generation/roles/chord_rhythm.h"
#include "src/generation/roles/harmonic_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

uint8_t countBits(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count = static_cast<uint8_t>(count + (mask & 1u));
    mask = static_cast<StepMask>(mask >> 1u);
  }
  return count;
}

bool samePlan(const HarmonicRhythmPlan& first,
              const HarmonicRhythmPlan& second) {
  return first.progression == second.progression &&
         first.onsets == second.onsets &&
         first.eventCount == second.eventCount &&
         first.phraseBarOrdinal == second.phraseBarOrdinal &&
         first.phraseHarmonicPosition == second.phraseHarmonicPosition;
}

bool sameResult(const HarmonicRhythmResult& first,
                const HarmonicRhythmResult& second) {
  return first.status == second.status && samePlan(first.plan, second.plan);
}

HarmonicRhythmResult planFor(ProgressionId progression,
                             uint8_t harmonicEventCount = 0) {
  HarmonicRhythmRequest request{};
  request.progression = progression;
  request.harmonicEventCount = harmonicEventCount;
  return realizeHarmonicRhythm(request);
}

void testStaticHarmonyCanHaveRepeatedChordAttacks() {
  const StepMask chordAttacks = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
  const HarmonicRhythmResult harmony = planFor(ProgressionId::StaticModal);

  assert(harmony.status == HarmonicRhythmStatus::Ok);
  assert(harmony.plan.eventCount == 1);
  assert(harmony.plan.onsets == stepBit(0));
  assert(countBits(chordAttacks) == 4);
  assert(countBits(chordAttacks) != harmony.plan.eventCount);
}

void testTwoHarmonicStatesAllowArbitraryArticulation() {
  const StepMask chordAttacks = static_cast<StepMask>(
      stepBit(0) | stepBit(2) | stepBit(4) | stepBit(6) |
      stepBit(8) | stepBit(10) | stepBit(12) | stepBit(14));
  const HarmonicRhythmResult harmony =
      planFor(ProgressionId::PopCycle, 2);

  assert(harmony.status == HarmonicRhythmStatus::Ok);
  assert(harmony.plan.eventCount == 2);
  assert(harmony.plan.onsets ==
         static_cast<StepMask>(stepBit(0) | stepBit(8)));
  assert(countBits(chordAttacks) == 8);
}

void testChordRhythmCannotChangeHarmonicTopology() {
  const ChordRhythmPlan held{
      ChordRhythmId::WholeBarHold,
      stepBit(0),
      static_cast<StepMask>(~stepBit(0)),
      0};
  const ChordRhythmPlan stabs{
      ChordRhythmId::OffbeatStab,
      static_cast<StepMask>(stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14)),
      0,
      0};

  const HarmonicRhythmResult first = planFor(ProgressionId::PopCycle);
  const HarmonicRhythmResult second = planFor(ProgressionId::PopCycle);

  assert(held.onsets != stabs.onsets);
  assert(samePlan(first.plan, second.plan));
  assert(first.plan.onsets == static_cast<StepMask>(stepBit(0) | stepBit(8)));
}

void testMovingProgressionCanChangeWithoutChordTopologyChange() {
  const StepMask unchangedChordAttacks = static_cast<StepMask>(
      stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14));
  const HarmonicRhythmResult pop = planFor(ProgressionId::PopCycle);
  const HarmonicRhythmResult minor = planFor(ProgressionId::MinorFall);

  assert(pop.status == HarmonicRhythmStatus::Ok);
  assert(minor.status == HarmonicRhythmStatus::Ok);
  assert(pop.plan.progression != minor.plan.progression);
  assert(pop.plan.onsets == minor.plan.onsets);
  assert(countBits(unchangedChordAttacks) == 4);
}

void testHeldArticulationDoesNotImplyProgressionSemantics() {
  const HarmonicRhythmResult staticHarmony = planFor(ProgressionId::PedalDrone);
  const HarmonicRhythmResult movingHarmony = planFor(ProgressionId::TwoFiveOne);

  assert(staticHarmony.plan.eventCount == 1);
  assert(movingHarmony.plan.eventCount == 2);
  assert(staticHarmony.plan.onsets == stepBit(0));
  assert(movingHarmony.plan.onsets ==
         static_cast<StepMask>(stepBit(0) | stepBit(8)));
}

void testExplicitCountsAndDeterminism() {
  const HarmonicRhythmResult thirds = planFor(ProgressionId::TwoFiveOne, 3);
  assert(thirds.status == HarmonicRhythmStatus::Ok);
  assert(thirds.plan.onsets == static_cast<StepMask>(
      stepBit(0) | stepBit(5) | stepBit(10)));

  const HarmonicRhythmResult quarters = planFor(ProgressionId::BorrowedLift, 4);
  assert(quarters.status == HarmonicRhythmStatus::Ok);
  assert(quarters.plan.onsets == static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12)));

  for (uint8_t i = 0; i < 32; ++i) {
    HarmonicRhythmRequest request{};
    request.progression = ProgressionId::ParallelShift;
    request.harmonicEventCount = 2;
    request.phraseBarOrdinal = i;
    request.phraseHarmonicPosition = static_cast<uint8_t>(31 - i);
    const HarmonicRhythmResult first = realizeHarmonicRhythm(request);
    const HarmonicRhythmResult second = realizeHarmonicRhythm(request);
    assert(sameResult(first, second));
    assert(first.plan.onsets == static_cast<StepMask>(stepBit(0) | stepBit(8)));
    assert(first.plan.phraseBarOrdinal == i);
    assert(first.plan.phraseHarmonicPosition == static_cast<uint8_t>(31 - i));
  }
}

void testInvalidRequestsAreRejected() {
  HarmonicRhythmRequest autoProgression{};
  assert(realizeHarmonicRhythm(autoProgression).status ==
         HarmonicRhythmStatus::InvalidRequest);

  HarmonicRhythmRequest tooMany{};
  tooMany.progression = ProgressionId::PopCycle;
  tooMany.harmonicEventCount = static_cast<uint8_t>(kMaxHarmonicEvents + 1);
  assert(realizeHarmonicRhythm(tooMany).status ==
         HarmonicRhythmStatus::InvalidRequest);
}

}  // namespace

int main() {
  testStaticHarmonyCanHaveRepeatedChordAttacks();
  testTwoHarmonicStatesAllowArbitraryArticulation();
  testChordRhythmCannotChangeHarmonicTopology();
  testMovingProgressionCanChangeWithoutChordTopologyChange();
  testHeldArticulationDoesNotImplyProgressionSemantics();
  testExplicitCountsAndDeterminism();
  testInvalidRequestsAreRejected();
  return 0;
}
