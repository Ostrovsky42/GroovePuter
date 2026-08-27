#include <cstdint>
#include <cstdio>

// P1R-T0 is characterization-only. Include the frozen H1 owner implementation
// directly so expected source semantics come from its authoritative selected
// Grammar rather than from a duplicated test-side progression table.
#include "../src/generation/roles/chord_progression.cpp"

namespace GroovePuterRhythm {
namespace {

constexpr int kSemanticGapExitCode = 42;

bool sameEvent(const HarmonicEvent& left, const HarmonicEvent& right) {
  return left.degree == right.degree &&
         left.quality == right.quality &&
         left.rootOffsetSemitones == right.rootOffsetSemitones;
}

void printEvent(const HarmonicEvent& value) {
  std::printf("degree=%u quality=%u rootOffset=%d",
              static_cast<unsigned>(value.degree),
              static_cast<unsigned>(value.quality),
              static_cast<int>(value.rootOffsetSemitones));
}

struct Case {
  ProgressionId id;
  uint8_t expectedPeriod;
};

constexpr Case kCases[] = {
    {ProgressionId::StaticModal, 1},
    {ProgressionId::PedalDrone, 1},
    {ProgressionId::PopCycle, 4},
    {ProgressionId::TwoFiveOne, 3},
    {ProgressionId::ParallelShift, 4},
    {ProgressionId::MinorFall, 4},
    {ProgressionId::BorrowedLift, 4},
};

constexpr uint8_t kMandatoryTwoFiveOneOrdinals[] = {7, 8, 9, 11, 14, 15};

bool isMandatoryTwoFiveOneOrdinal(uint8_t ordinal) {
  for (uint8_t value : kMandatoryTwoFiveOneOrdinals) {
    if (value == ordinal) return true;
  }
  return false;
}

int characterizeCase(const Case& testCase,
                     uint16_t& totalMismatches,
                     uint16_t& twoFiveOneMismatches,
                     bool (&mandatoryObserved)[sizeof(kMandatoryTwoFiveOneOrdinals)]) {
  ChordProgressionRequest request{};
  request.requestedId = testCase.id;
  request.family = RhythmFamily::FourFloor;
  request.generation.projectSeed = 0x50315254u;  // "P1RT"
  request.generation.phraseOrdinal = 0x1234u;
  request.harmonicEventCount = kMaxHarmonicEvents;
  request.phraseBars = 8;

  const GrammarSet* grammarSet = grammarSetFor(testCase.id);
  if (grammarSet == nullptr || grammarSet->count == 0) {
    std::printf("P1R-T0 SETUP FAIL progression=%s missing grammar set\n",
                chordProgressionName(testCase.id));
    return 2;
  }
  for (uint8_t variant = 0; variant < grammarSet->count; ++variant) {
    if (grammarSet->variants[variant].count != testCase.expectedPeriod) {
      std::printf(
          "P1R-T0 OWNER PERIOD FAIL progression=%s variant=%u expected=%u actual=%u\n",
          chordProgressionName(testCase.id), static_cast<unsigned>(variant),
          static_cast<unsigned>(testCase.expectedPeriod),
          static_cast<unsigned>(grammarSet->variants[variant].count));
      return 2;
    }
  }

  const Grammar* selected = selectGrammar(request, testCase.id);
  if (selected == nullptr || selected->count != testCase.expectedPeriod) {
    std::printf("P1R-T0 SELECT FAIL progression=%s expectedPeriod=%u\n",
                chordProgressionName(testCase.id),
                static_cast<unsigned>(testCase.expectedPeriod));
    return 2;
  }

  const ChordProgressionResult realized = realizeChordProgression(request);
  const bool staticProgression = testCase.id == ProgressionId::StaticModal ||
                                 testCase.id == ProgressionId::PedalDrone;
  const ChordProgressionStatus expectedStatus =
      staticProgression ? ChordProgressionStatus::ValidButStatic
                        : ChordProgressionStatus::Ok;
  if (realized.status != expectedStatus || realized.plan.id != testCase.id ||
      realized.plan.eventCount == 0) {
    std::printf(
        "P1R-T0 PLAN FAIL progression=%s status=%u eventCount=%u\n",
        chordProgressionName(testCase.id),
        static_cast<unsigned>(realized.status),
        static_cast<unsigned>(realized.plan.eventCount));
    return 2;
  }

  std::printf("P1R-T0 progression=%s intrinsicPeriod=%u publicPlanCount=%u\n",
              chordProgressionName(testCase.id),
              static_cast<unsigned>(selected->count),
              static_cast<unsigned>(realized.plan.eventCount));

  for (uint8_t ordinal = 0; ordinal < 16; ++ordinal) {
    const HarmonicEvent& expected = selected->events[ordinal % selected->count];
    const HarmonicEvent& projected =
        realized.plan.events[ordinal % realized.plan.eventCount];
    const bool matches = sameEvent(expected, projected);

    if (testCase.id == ProgressionId::TwoFiveOne &&
        isMandatoryTwoFiveOneOrdinal(ordinal)) {
      for (uint8_t index = 0;
           index < sizeof(kMandatoryTwoFiveOneOrdinals); ++index) {
        if (kMandatoryTwoFiveOneOrdinals[index] == ordinal) {
          mandatoryObserved[index] = true;
          break;
        }
      }
      std::printf("P1R-T0 mandatory TwoFiveOne ordinal=%u match=%s\n",
                  static_cast<unsigned>(ordinal), matches ? "yes" : "NO");
    }

    if (!matches) {
      ++totalMismatches;
      if (testCase.id == ProgressionId::TwoFiveOne) {
        ++twoFiveOneMismatches;
      } else {
        std::printf("P1R-T0 UNEXPECTED MISMATCH progression=%s ordinal=%u ",
                    chordProgressionName(testCase.id),
                    static_cast<unsigned>(ordinal));
        std::printf("expected[");
        printEvent(expected);
        std::printf("] projected[");
        printEvent(projected);
        std::printf("]\n");
        return 2;
      }

      std::printf("P1R-T0 SOURCE PERIOD MISMATCH progression=%s ordinal=%u ",
                  chordProgressionName(testCase.id),
                  static_cast<unsigned>(ordinal));
      std::printf("expected[");
      printEvent(expected);
      std::printf("] projected[");
      printEvent(projected);
      std::printf("]\n");
    }
  }

  return 0;
}

}  // namespace
}  // namespace GroovePuterRhythm

int main() {
  using namespace GroovePuterRhythm;

  uint16_t totalMismatches = 0;
  uint16_t twoFiveOneMismatches = 0;
  bool mandatoryObserved[sizeof(kMandatoryTwoFiveOneOrdinals)]{};

  for (const Case& testCase : kCases) {
    const int result = characterizeCase(testCase, totalMismatches,
                                        twoFiveOneMismatches,
                                        mandatoryObserved);
    if (result != 0) return result;
  }

  for (bool observed : mandatoryObserved) {
    if (!observed) {
      std::printf("P1R-T0 SETUP FAIL mandatory TwoFiveOne ordinal not observed\n");
      return 2;
    }
  }

  if (totalMismatches == 0) {
    std::printf(
        "P1R-T0 RESULT: PUBLIC_PLAN_PRESERVES_GLOBAL_SOURCE_PERIOD ordinals=0..15\n");
    return 0;
  }

  if (totalMismatches != twoFiveOneMismatches ||
      twoFiveOneMismatches != 8) {
    std::printf(
        "P1R-T0 CHARACTERIZATION FAIL totalMismatches=%u TwoFiveOneMismatches=%u\n",
        static_cast<unsigned>(totalMismatches),
        static_cast<unsigned>(twoFiveOneMismatches));
    return 2;
  }

  std::printf(
      "P1R-T0 RESULT: H1_SOURCE_PERIOD_NOT_REPRESENTABLE publicPlanMax=%u TwoFiveOnePeriod=3 mismatches=%u\n",
      static_cast<unsigned>(kMaxHarmonicEvents),
      static_cast<unsigned>(twoFiveOneMismatches));
  return kSemanticGapExitCode;
}
