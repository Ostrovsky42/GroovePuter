#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/phrase/runtime_pattern_event_bank.h"

namespace {

using namespace PhraseRuntime;

SynthPattern emptyPattern() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
  }
  return pattern;
}

PatternProjectionSettings settings() {
  PatternProjectionSettings value{};
  value.synthIndex = 0;
  value.swingPercent = 75;
  value.swingEnabled = true;
  value.gateLengthRatio = 0.5f;
  return value;
}

void testSourceStepLookupSurvivesTimingProjection() {
  RuntimePatternEventBank bank{};
  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 40;
  pattern.steps[0].timing = -23;
  pattern.steps[3].note = 43;
  pattern.steps[3].timing = 17;
  pattern.steps[4].note = -2;  // TIE is lifetime input, not an onset event.
  pattern.steps[15].note = 55;
  pattern.steps[15].timing = 23;

  assert(bank.refresh(0, 0, 0, pattern, settings()) ==
         PatternBankRefreshStatus::Ready);
  const RuntimePatternEventBuffer& compact = bank.select(0, 0, 0);
  assert(compact.count == 3);
  assert(compact.onsetMask == static_cast<uint16_t>(
      (1u << 0u) | (1u << 3u) | (1u << 15u)));

  const RuntimeSynthEvent* step0 = compact.eventForSourceStep(0);
  const RuntimeSynthEvent* step3 = compact.eventForSourceStep(3);
  const RuntimeSynthEvent* step15 = compact.eventForSourceStep(15);
  assert(step0 != nullptr && step0->note == 40);
  assert(step3 != nullptr && step3->note == 43);
  assert(step15 != nullptr && step15->note == 55);

  assert(compact.eventForSourceStep(1) == nullptr);
  assert(compact.eventForSourceStep(4) == nullptr);
  assert(compact.eventForSourceStep(16) == nullptr);

  // The lookup identity is the physical source step, not projected startTick.
  assert(step0->startTick != 0);
  assert(step15->startTick != 15 * 24);
}

void testAllSixteenPhysicalOnsetsRemainAddressable() {
  RuntimePatternEventBank bank{};
  SynthPattern pattern = emptyPattern();
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(48 + step);
  }
  PatternProjectionSettings value{};
  value.synthIndex = 0;
  value.swingPercent = 50;
  value.swingEnabled = false;
  value.gateLengthRatio = 0.5f;
  assert(bank.refresh(0, 0, 1, pattern, value) ==
         PatternBankRefreshStatus::Ready);

  const RuntimePatternEventBuffer& compact = bank.select(0, 0, 1);
  assert(compact.onsetMask == 0xFFFFu);
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const RuntimeSynthEvent* event = compact.eventForSourceStep(step);
    assert(event != nullptr);
    assert(event->note == static_cast<uint8_t>(48 + step));
  }
}

void testOrderingMetadataKeepsCompactBudget() {
  static_assert(sizeof(decltype(RuntimePatternEventBuffer{}.onsetMask)) ==
                    sizeof(uint16_t),
                "physical onset identity must remain exactly 16 bits");
  static_assert(sizeof(RuntimePatternEventBuffer) <= 164,
                "source-step ordering metadata grew Pattern carrier too far");
  static_assert(sizeof(RuntimePatternEventBank) <= 5500,
                "source-step ordering metadata exceeded accepted P2 bank budget");
}

}  // namespace

int main() {
  testSourceStepLookupSurvivesTimingProjection();
  testAllSixteenPhysicalOnsetsRemainAddressable();
  testOrderingMetadataKeepsCompactBudget();
  std::puts("PATTERN/PHRASE P2 source-step ordering carrier: PASS");
  return 0;
}
