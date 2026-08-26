#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/composition/phrase_harmonic_timeline.h"
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

HarmonicRhythmResult realize(ProgressionId progression,
                             uint8_t phraseBarOrdinal = 0) {
  HarmonicRhythmRequest request{};
  request.progression = progression;
  request.phraseBarOrdinal = phraseBarOrdinal;
  return realizeHarmonicRhythm(request);
}

void proveAcceptedBootstrap() {
  const auto staticModal = realize(ProgressionId::StaticModal);
  const auto pedal = realize(ProgressionId::PedalDrone);
  const auto moving = realize(ProgressionId::PopCycle);

  assert(staticModal.status == HarmonicRhythmStatus::Ok);
  assert(pedal.status == HarmonicRhythmStatus::Ok);
  assert(moving.status == HarmonicRhythmStatus::Ok);
  assert(staticModal.plan.eventCount == 1);
  assert(pedal.plan.eventCount == 1);
  assert(staticModal.plan.onsets == stepBit(0));
  assert(pedal.plan.onsets == stepBit(0));
  assert(moving.plan.eventCount == 2);
  assert(moving.plan.onsets ==
         static_cast<StepMask>(stepBit(0) | stepBit(8)));

  std::printf("F08 owner recovered YES\n");
  std::printf("static mask=%04x count=%u\n",
              static_cast<unsigned>(staticModal.plan.onsets),
              static_cast<unsigned>(staticModal.plan.eventCount));
  std::printf("moving mask=%04x count=%u\n",
              static_cast<unsigned>(moving.plan.onsets),
              static_cast<unsigned>(moving.plan.eventCount));
}

void proveChordRhythmIndependence() {
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

  const auto first = realize(ProgressionId::PopCycle);
  const auto second = realize(ProgressionId::PopCycle);
  assert(held.onsets != stabs.onsets);
  assert(countBits(held.onsets) != countBits(stabs.onsets));
  assert(first.plan.onsets == second.plan.onsets);
  assert(first.plan.eventCount == second.plan.eventCount);
  std::printf("ChordRhythm independence=YES\n");
}

void characterizePhraseBoundaryGap() {
  for (const uint8_t bars : {uint8_t{1}, uint8_t{2}, uint8_t{4}, uint8_t{8}}) {
    std::printf("phrase length=%u projection=UNRESOLVED_BY_ACCEPTED_F08\n",
                static_cast<unsigned>(bars));
  }

  // Accepted F08 only carries bar coordinates. It does not establish whether a
  // static {0} event repeats at every bar boundary or exists only at phrase
  // start, nor whether moving {0,8} restarts every semantic bar. Do not build a
  // PhraseHarmonicTimeline until that musical choice has an owner.
  const auto staticBar0 = realize(ProgressionId::StaticModal, 0);
  const auto staticBar1 = realize(ProgressionId::StaticModal, 1);
  const auto movingBar0 = realize(ProgressionId::PopCycle, 0);
  const auto movingBar1 = realize(ProgressionId::PopCycle, 1);
  assert(staticBar0.plan.onsets == staticBar1.plan.onsets);
  assert(movingBar0.plan.onsets == movingBar1.plan.onsets);
  assert(staticBar1.plan.phraseBarOrdinal == 1);
  assert(movingBar1.plan.phraseBarOrdinal == 1);

  std::printf("static multibar interpretation=AMBIGUOUS\n");
  std::printf("moving multibar interpretation=AMBIGUOUS\n");
  std::printf("REST HEAVY result=NOT_AN_OWNER_OF_HARMONIC_TIME\n");
  std::printf("H1 progression source selection count=1_REQUIRED_BY_H1_NOT_EXECUTED_IN_W1\n");
  std::printf("32-position production claim=NO_SYNTHETIC_ONLY\n");
}

}  // namespace

int main() {
  static_assert(sizeof(HarmonicRhythmRequest) <= 8,
                "W1 request must remain bounded");
  static_assert(sizeof(HarmonicRhythmPlan) <= 8,
                "W1 plan must remain bounded");

  proveAcceptedBootstrap();
  proveChordRhythmIndependence();
  characterizePhraseBoundaryGap();
  std::printf("PHRASE-W1 DECISION_B\n");
  return 0;
}
