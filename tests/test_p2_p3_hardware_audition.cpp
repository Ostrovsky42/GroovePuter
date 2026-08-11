#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/migration/p2_p3_hardware_audition.h"
#include "src/generation/roles/chord_rhythm_retrigger.h"

using namespace GroovePuterRhythm;

namespace {

bool hasStep(StepMask mask, uint8_t step) {
  return (mask & stepBit(step)) != 0;
}

void testRetriggerFixture() {
  const auto plan = realizeP2P3HardwareAudition(
      P2P3HardwareAuditionFixture::Retrigger);
  assert(plan.status == P2P3HardwareAuditionStatus::Ok);
  assert(plan.barCount == 1);
  assert(plan.sourceAdvanceCount == 2);
  const auto& bar = plan.bars[0];
  assert(hasStep(bar.sourceAdvanceOnsets, 0));
  assert(hasStep(bar.sameChordRetriggers, 4));
  assert(hasStep(bar.sourceAdvanceOnsets, 8));
  assert(hasStep(bar.sameChordRetriggers, 12));
  assert(bar.sourceOrdinalByStep[0] == 0);
  assert(bar.sourceOrdinalByStep[4] == 0);
  assert(bar.sourceOrdinalByStep[8] == 1);
  assert(bar.sourceOrdinalByStep[12] == 1);
}

void testDenseRetriggerFixture() {
  const auto plan = realizeP2P3HardwareAudition(
      P2P3HardwareAuditionFixture::DenseRetrigger);
  assert(plan.status == P2P3HardwareAuditionStatus::Ok);
  assert(plan.barCount == 1);
  assert(plan.sourceAdvanceCount == 1);
  const auto& bar = plan.bars[0];
  for (uint8_t step = 0; step < 16; ++step) {
    assert(hasStep(bar.audibleOnsets, step));
    assert(bar.sourceOrdinalByStep[step] == 0);
  }
}

void testCrossBarHoldFixture() {
  const auto plan = realizeP2P3HardwareAudition(
      P2P3HardwareAuditionFixture::CrossBarHold);
  assert(plan.status == P2P3HardwareAuditionStatus::Ok);
  assert(plan.barCount == 2);
  assert(plan.sourceAdvanceCount == 1);
  const auto& a = plan.bars[0];
  const auto& b = plan.bars[1];
  assert(hasStep(a.sourceAdvanceOnsets, 12));
  assert(hasStep(a.continuations, 13));
  assert(hasStep(a.continuations, 15));
  assert(hasStep(b.continuations, 0));
  assert(hasStep(b.continuations, 3));
  assert(hasStep(b.releasePoints, 4));
  assert(b.audibleOnsets == 0);
}

void testMultiBarIncomingSourceFixture() {
  const auto plan = realizeP2P3HardwareAudition(
      P2P3HardwareAuditionFixture::MultiBarNS);
  assert(plan.status == P2P3HardwareAuditionStatus::Ok);
  assert(plan.barCount == 4);
  assert(plan.sourceAdvanceCount == 6);
  assert(plan.bars[0].sourceOrdinalByStep[0] == 0);
  assert(plan.bars[0].sourceOrdinalByStep[4] == 0);
  assert(plan.bars[0].sourceOrdinalByStep[8] == 1);
  assert(plan.bars[0].sourceOrdinalByStep[12] == 1);
  assert(plan.bars[1].sourceOrdinalByStep[0] == 1);
  assert(plan.bars[1].sourceOrdinalByStep[8] == 2);
  assert(plan.bars[1].sourceOrdinalByStep[12] == 2);
  assert(plan.bars[2].sourceOrdinalByStep[0] == 3);
  assert(plan.bars[2].sourceOrdinalByStep[4] == 3);
  assert(plan.bars[2].sourceOrdinalByStep[8] == 4);
  assert(plan.bars[2].sourceOrdinalByStep[12] == 4);
  assert(plan.bars[3].sourceOrdinalByStep[0] == 4);
  assert(plan.bars[3].sourceOrdinalByStep[8] == 5);
  assert(plan.bars[3].sourceOrdinalByStep[12] == 5);
}

void testInvalidFixtureFailsClosed() {
  const auto plan = realizeP2P3HardwareAudition(
      P2P3HardwareAuditionFixture::Count);
  assert(plan.status == P2P3HardwareAuditionStatus::InvalidFixture);
  assert(plan.barCount == 0);
  assert(plan.sourceAdvanceCount == 0);
}

}  // namespace

int main() {
  testRetriggerFixture();
  testDenseRetriggerFixture();
  testCrossBarHoldFixture();
  testMultiBarIncomingSourceFixture();
  testInvalidFixtureFailsClosed();
  std::cout << "P2+P3 hardware audition composition: OK\n";
  return 0;
}
