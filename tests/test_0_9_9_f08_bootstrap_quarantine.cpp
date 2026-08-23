#include <cstdint>
#include <iostream>

#include "src/generation/roles/harmonic_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint8_t kFutureMinimumDistinctMovingClocks = 4;

constexpr ProgressionId kRepresentativeMovingProgressions[] = {
    ProgressionId::PopCycle,
    ProgressionId::TwoFiveOne,
    ProgressionId::ParallelShift,
    ProgressionId::MinorFall,
    ProgressionId::BorrowedLift,
};

// F08 BOOTSTRAP QUARANTINE
//
// Current implementation intentionally exposes only:
//   static -> {0}
//   moving -> {0,8}
//
// This is NOT accepted as the final harmonic-rhythm vocabulary.
//
// Remove/update this quarantine only when HarmonicRhythm is derived from
// musical progression/Phrase context and the corresponding musical acceptance
// has passed.
//
// This test is intentionally XFAIL-style without leaving CI red:
//   distinct moving clocks < target -> EXPECTED XFAIL, process exits 0
//   distinct moving clocks >= target -> XPASS, process exits non-zero
//
// XPASS is a review gate, not a failure of richer musical behavior. It forces
// the developer to remove or revise this quarantine instead of silently
// converting the F08 bootstrap into a permanent contract.

void printClock(StepMask mask) {
  std::cout << '{';
  bool first = true;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((mask & stepBit(step)) == 0) continue;
    if (!first) std::cout << ',';
    std::cout << static_cast<unsigned>(step);
    first = false;
  }
  std::cout << '}';
}

}  // namespace

int main() {
  constexpr uint8_t kRepresentativeCount =
      static_cast<uint8_t>(sizeof(kRepresentativeMovingProgressions) /
                           sizeof(kRepresentativeMovingProgressions[0]));

  StepMask distinctClocks[kRepresentativeCount]{};
  uint8_t distinctCount = 0;

  std::cout << "F08 BOOTSTRAP QUARANTINE\n";
  std::cout << "Representative moving-policy corpus:\n";

  for (ProgressionId progression : kRepresentativeMovingProgressions) {
    HarmonicRhythmRequest request{};
    request.progression = progression;
    const HarmonicRhythmResult result = realizeHarmonicRhythm(request);

    if (result.status != HarmonicRhythmStatus::Ok ||
        result.plan.eventCount == 0 ||
        result.plan.onsets == 0) {
      std::cerr << "HARD FAIL: invalid HarmonicRhythm result for "
                << chordProgressionName(progression) << '\n';
      return 2;
    }

    bool seen = false;
    for (uint8_t index = 0; index < distinctCount; ++index) {
      if (distinctClocks[index] == result.plan.onsets) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      distinctClocks[distinctCount++] = result.plan.onsets;
    }

    std::cout << "  " << chordProgressionName(progression) << " -> ";
    printClock(result.plan.onsets);
    std::cout << " (" << static_cast<unsigned>(result.plan.eventCount)
              << " events)\n";
  }

  std::cout << "distinctMovingHarmonicClocks="
            << static_cast<unsigned>(distinctCount)
            << " futureTarget>="
            << static_cast<unsigned>(kFutureMinimumDistinctMovingClocks)
            << '\n';

  if (distinctCount >= kFutureMinimumDistinctMovingClocks) {
    std::cerr
        << "XPASS: moving HarmonicRhythm vocabulary reached the quarantine "
           "target. Musical policy has evolved; remove/update the F08 bootstrap "
           "quarantine and perform explicit musical acceptance before changing "
           "the golden.\n";
    return 1;
  }

  std::cout
      << "EXPECTED XFAIL: moving HarmonicRhythm vocabulary remains below the "
         "future target. Current bootstrap debt is visible and CI stays green.\n"
      << "Debt: default moving policy is still intentionally collapsed; this "
         "is not the final musical contract.\n";
  return 0;
}
