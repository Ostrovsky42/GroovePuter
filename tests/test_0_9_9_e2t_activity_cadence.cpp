#include "../src/generation/rhythm/evolution_activity.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
using GroovePuterRhythm::EvolutionActivity;
using GroovePuterRhythm::EvolutionCadenceDecision;
using GroovePuterRhythm::GenerationContext;
using GroovePuterRhythm::evolutionCadenceDecision;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "E2T FAIL: %s\n", message);
    std::exit(1);
  }
}

uint16_t cadenceMask(EvolutionActivity activity,
                     const GenerationContext& generation) {
  uint16_t mask = 0;
  for (uint8_t evolutionOrdinal = 0; evolutionOrdinal < 16;
       ++evolutionOrdinal) {
    const uint8_t phraseBarOrdinal =
        static_cast<uint8_t>(evolutionOrdinal * 4u);
    if (evolutionCadenceDecision(activity,
                                 phraseBarOrdinal,
                                 evolutionOrdinal,
                                 generation) ==
        EvolutionCadenceDecision::Attempt) {
      mask = static_cast<uint16_t>(mask | (1u << evolutionOrdinal));
    }
  }
  return mask;
}

void checkEightBarFourPlusFour(const GenerationContext& generation) {
  for (uint8_t phraseBarOrdinal = 0; phraseBarOrdinal < 8;
       ++phraseBarOrdinal) {
    const uint8_t evolutionOrdinal =
        static_cast<uint8_t>(phraseBarOrdinal / 4u);
    const EvolutionCadenceDecision decision = evolutionCadenceDecision(
        EvolutionActivity::High,
        phraseBarOrdinal,
        evolutionOrdinal,
        generation);
    const bool expectedAttempt =
        phraseBarOrdinal == 0u || phraseBarOrdinal == 4u;
    expect((decision == EvolutionCadenceDecision::Attempt) == expectedAttempt,
           "8 bars must remain two 4-bar cadence decision segments");
  }
}

}  // namespace

int main() {
  static_assert(GroovePuterRhythm::kEvolutionCadenceSegmentBars == 4,
                "E2t cadence segment bound changed");

  const GenerationContext baseGeneration{0x0E2C0A9Eu, 17u};
  const GenerationContext changedProjectSeed{0x0E2C0A9Fu, 17u};
  const GenerationContext changedPhraseOrdinal{0x0E2C0A9Eu, 18u};

  const uint16_t offMask = cadenceMask(EvolutionActivity::Off, baseGeneration);
  const uint16_t lowMask = cadenceMask(EvolutionActivity::Low, baseGeneration);
  const uint16_t mediumMask =
      cadenceMask(EvolutionActivity::Medium, baseGeneration);
  const uint16_t highMask = cadenceMask(EvolutionActivity::High, baseGeneration);

  expect(offMask == 0x0000u, "OFF must never permit an attempt");
  expect(lowMask == 0xA401u, "LOW deterministic cadence contract changed");
  expect(mediumMask == 0xA6D1u,
         "MEDIUM deterministic cadence contract changed");
  expect(highMask == 0xFFFFu,
         "HIGH must permit every valid evolution segment boundary");
  expect((lowMask & static_cast<uint16_t>(~mediumMask)) == 0u,
         "LOW attempts must remain a subset of MEDIUM attempts");
  expect((mediumMask & static_cast<uint16_t>(~highMask)) == 0u,
         "MEDIUM attempts must remain a subset of HIGH attempts");

  expect(cadenceMask(EvolutionActivity::Low, baseGeneration) == lowMask,
         "same activity/coordinates/context must be bit-identical");
  expect(cadenceMask(EvolutionActivity::Low, changedProjectSeed) == 0x8420u,
         "project seed must participate in deterministic cadence context");
  expect(cadenceMask(EvolutionActivity::Low, changedPhraseOrdinal) == 0x1040u,
         "phrase ordinal must participate in deterministic cadence context");

  checkEightBarFourPlusFour(baseGeneration);

  expect(evolutionCadenceDecision(EvolutionActivity::High,
                                  4u,
                                  0u,
                                  baseGeneration) ==
             EvolutionCadenceDecision::Hold,
         "inconsistent E0a temporal coordinates must fail closed to HOLD");
  expect(evolutionCadenceDecision(EvolutionActivity::High,
                                  5u,
                                  1u,
                                  baseGeneration) ==
             EvolutionCadenceDecision::Hold,
         "non-boundary physical bars must remain HOLD");
  expect(evolutionCadenceDecision(
             static_cast<EvolutionActivity>(
                 static_cast<uint8_t>(EvolutionActivity::Count)),
             0u,
             0u,
             baseGeneration) == EvolutionCadenceDecision::Hold,
         "invalid activity must fail closed to HOLD");

  std::printf("E2T_ACTIVITY OFF=%04X LOW=%04X MEDIUM=%04X HIGH=%04X\n",
              offMask,
              lowMask,
              mediumMask,
              highMask);
  std::puts("E2T_8_BAR 0..3=segment0 4..7=segment1 decision-points={0,4}");
  std::puts("0.9.9-E2t activity/cadence: PASS");
  return 0;
}
