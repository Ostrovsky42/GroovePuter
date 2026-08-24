#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "src/generation/roles/chord_progression.h"
#include "src/generation/roles/harmonic_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

struct CorpusCase {
  ProgressionId progression;
  uint32_t projectSeed;
  uint16_t phraseOrdinal;
  StepMask expectedOnsets;
  uint8_t expectedEventCount;
};

bool sameProgressionPlan(const ChordProgressionPlan& left,
                         const ChordProgressionPlan& right) {
  if (left.id != right.id || left.eventCount != right.eventCount) return false;
  for (uint8_t index = 0; index < left.eventCount; ++index) {
    const HarmonicEvent& a = left.events[index];
    const HarmonicEvent& b = right.events[index];
    if (a.degree != b.degree || a.quality != b.quality ||
        a.rootOffsetSemitones != b.rootOffsetSemitones) {
      return false;
    }
  }
  return true;
}

void printMask(StepMask mask) {
  std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<uint16_t>(mask) << std::dec;
}

}  // namespace

int main() {
  const CorpusCase cases[] = {
      {ProgressionId::StaticModal, 0x13579bdfu, 3u, stepBit(0), 1},
      {ProgressionId::PedalDrone, 0x2468ace0u, 5u, stepBit(0), 1},
      {ProgressionId::PopCycle, 0x31415926u, 7u,
       static_cast<StepMask>(stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12)), 4},
      {ProgressionId::TwoFiveOne, 0x27182818u, 11u,
       static_cast<StepMask>(stepBit(0) | stepBit(6) | stepBit(10)), 3},
      {ProgressionId::ParallelShift, 0x0badf00du, 13u,
       static_cast<StepMask>(stepBit(0) | stepBit(8)), 2},
      {ProgressionId::MinorFall, 0xc001d00du, 17u,
       static_cast<StepMask>(stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12)), 4},
      {ProgressionId::BorrowedLift, 0x5eed1234u, 19u,
       static_cast<StepMask>(stepBit(0) | stepBit(12)), 2},
  };

  std::cout << "F08.1 FIXED-SEED HARMONIC VOCABULARY CORPUS\n";

  for (const CorpusCase& item : cases) {
    HarmonicRhythmRequest harmonicRequest{};
    harmonicRequest.progression = item.progression;
    const HarmonicRhythmResult firstHarmonic =
        realizeHarmonicRhythm(harmonicRequest);
    const HarmonicRhythmResult secondHarmonic =
        realizeHarmonicRhythm(harmonicRequest);

    assert(firstHarmonic.status == HarmonicRhythmStatus::Ok);
    assert(secondHarmonic.status == HarmonicRhythmStatus::Ok);
    assert(firstHarmonic.plan.onsets == secondHarmonic.plan.onsets);
    assert(firstHarmonic.plan.eventCount == secondHarmonic.plan.eventCount);
    assert(firstHarmonic.plan.onsets == item.expectedOnsets);
    assert(firstHarmonic.plan.eventCount == item.expectedEventCount);

    ChordProgressionRequest progressionRequest{};
    progressionRequest.requestedId = item.progression;
    progressionRequest.family = RhythmFamily::FourFloor;
    progressionRequest.generation.projectSeed = item.projectSeed;
    progressionRequest.generation.phraseOrdinal = item.phraseOrdinal;
    progressionRequest.harmonicEventCount = firstHarmonic.plan.eventCount;
    progressionRequest.phraseBars = 1;

    const ChordProgressionResult firstProgression =
        realizeChordProgression(progressionRequest);
    const ChordProgressionResult secondProgression =
        realizeChordProgression(progressionRequest);
    const bool staticHarmony = isStaticHarmonicProgression(item.progression);
    const ChordProgressionStatus expectedStatus = staticHarmony
        ? ChordProgressionStatus::ValidButStatic
        : ChordProgressionStatus::Ok;

    assert(firstProgression.status == expectedStatus);
    assert(secondProgression.status == expectedStatus);
    assert(sameProgressionPlan(firstProgression.plan, secondProgression.plan));
    assert(firstProgression.plan.eventCount == item.expectedEventCount);

    std::cout << "  seed=0x" << std::hex << std::setw(8) << std::setfill('0')
              << item.projectSeed << std::dec
              << " phrase=" << item.phraseOrdinal
              << " progression=" << chordProgressionName(item.progression)
              << " clock=";
    printMask(firstHarmonic.plan.onsets);
    std::cout << " events=" << static_cast<unsigned>(firstHarmonic.plan.eventCount)
              << '\n';
  }

  std::cout << "F08.1 fixed-seed corpus: OK\n";
  return 0;
}
