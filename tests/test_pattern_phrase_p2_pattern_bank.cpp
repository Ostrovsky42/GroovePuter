#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "src/phrase/runtime_pattern_event_bank.h"

namespace {

using namespace PhraseRuntime;

SynthPattern emptyPattern() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
  }
  return pattern;
}

PatternProjectionSettings settingsFor(uint8_t synthIndex,
                                      float gateLengthRatio = 0.5f,
                                      uint8_t swingPercent = 50,
                                      bool swingEnabled = false) {
  PatternProjectionSettings settings{};
  settings.synthIndex = synthIndex;
  settings.gateLengthRatio = gateLengthRatio;
  settings.swingPercent = swingPercent;
  settings.swingEnabled = swingEnabled;
  return settings;
}

void assertMatchesP1CProjection(const SynthPattern& pattern,
                                const PatternProjectionSettings& settings,
                                const RuntimePatternEventBuffer& compact) {
  RuntimeSynthEventBuffer reference{};
  uint8_t referenceSourceSteps[SynthPattern::kSteps]{};
  assert(projectPatternToRuntimeEventsWithSourceSteps(
             pattern, settings, reference, referenceSourceSteps) ==
         PatternProjectionStatus::Ready);
  assert(reference.count <= SynthPattern::kSteps);
  assert(compact.count == reference.count);
  assert(compact.lengthTicks() == kTicksPerBar);

  uint16_t expectedMask = 0;
  for (uint8_t i = 0; i < reference.count; ++i) {
    const uint8_t sourceStep = referenceSourceSteps[i];
    assert(sourceStep < SynthPattern::kSteps);
    expectedMask = static_cast<uint16_t>(
        expectedMask | static_cast<uint16_t>(1u << sourceStep));
    const RuntimeSynthEvent* retained = compact.eventForSourceStep(sourceStep);
    assert(retained != nullptr);
    assert(std::memcmp(retained, &reference.events[i],
                       sizeof(RuntimeSynthEvent)) == 0);
  }
  assert(compact.onsetMask == expectedMask);
}

void testCompactCarrierBudgetAndAbi() {
  static_assert(std::is_trivially_copyable<RuntimePatternEventBuffer>::value,
                "retained Pattern carrier must remain trivially copyable");
  static_assert(std::is_trivially_copyable<RuntimePatternEventBank>::value,
                "retained Pattern bank must remain trivially copyable");
  static_assert(kPatternRuntimeMaxEvents == SynthPattern::kSteps,
                "physical Pattern retained capacity must stay at 16 events");
  static_assert(sizeof(RuntimePatternEventBuffer) <= 164,
                "compact Pattern carrier unexpectedly grew");
  static_assert(sizeof(RuntimePatternEventBank) <= 5500,
                "retained Pattern bank exceeds P2 fixed-memory budget");
  static_assert(sizeof(RuntimePatternEventBank) <
                    8 * sizeof(RuntimeSynthEventBuffer),
                "P2 must not retain PHRASE-sized buffers per Pattern slot");
}

void testEveryResidentAddressIsIndependent() {
  RuntimePatternEventBank bank{};
  for (uint8_t synth = 0; synth < 2; ++synth) {
    for (uint8_t physicalBank = 0; physicalBank < kBankCount; ++physicalBank) {
      for (uint8_t patternIndex = 0;
           patternIndex < Bank<SynthPattern>::kPatterns;
           ++patternIndex) {
        SynthPattern pattern = emptyPattern();
        const uint8_t slot = static_cast<uint8_t>(
            physicalBank * Bank<SynthPattern>::kPatterns + patternIndex);
        pattern.steps[slot % SynthPattern::kSteps].note =
            static_cast<int8_t>(24 + synth * 32 + slot);
        pattern.steps[slot % SynthPattern::kSteps].velocity =
            static_cast<uint8_t>(64 + slot);
        const auto settings = settingsFor(synth);
        assert(bank.refresh(synth, physicalBank, patternIndex, pattern, settings) ==
               PatternBankRefreshStatus::Ready);
      }
    }
  }

  for (uint8_t synth = 0; synth < 2; ++synth) {
    for (uint8_t physicalBank = 0; physicalBank < kBankCount; ++physicalBank) {
      for (uint8_t patternIndex = 0;
           patternIndex < Bank<SynthPattern>::kPatterns;
           ++patternIndex) {
        const auto& selected = bank.select(synth, physicalBank, patternIndex);
        assert(selected.count == 1);
        const uint8_t slot = static_cast<uint8_t>(
            physicalBank * Bank<SynthPattern>::kPatterns + patternIndex);
        assert(selected.events[0].note ==
               static_cast<uint8_t>(24 + synth * 32 + slot));
        assert(selected.events[0].velocity == static_cast<uint8_t>(64 + slot));
      }
    }
  }
}

void testCompactProjectionMatchesP1CExactly() {
  RuntimePatternEventBank bank{};
  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 48;
  pattern.steps[0].timing = -23;
  pattern.steps[0].velocity = 81;
  pattern.steps[0].accent = true;
  pattern.steps[7].note = 60;
  pattern.steps[7].slide = true;
  pattern.steps[7].ghost = true;
  pattern.steps[7].probability = 73;
  pattern.steps[7].fx = 2;
  pattern.steps[7].fxParam = 4;
  pattern.steps[15].note = 67;
  pattern.steps[15].timing = 23;

  const auto settings = settingsFor(1, 0.73f, 75, true);
  assert(bank.refresh(1, 1, 7, pattern, settings) ==
         PatternBankRefreshStatus::Ready);
  assertMatchesP1CProjection(pattern, settings, bank.select(1, 1, 7));
}

void testSixteenOnsetsFitWithoutPhraseCapacity() {
  RuntimePatternEventBank bank{};
  SynthPattern pattern = emptyPattern();
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(36 + step);
    pattern.steps[step].velocity = static_cast<uint8_t>(70 + step);
  }
  const auto settings = settingsFor(0);
  assert(bank.refresh(0, 0, 0, pattern, settings) ==
         PatternBankRefreshStatus::Ready);
  const auto& selected = bank.select(0, 0, 0);
  assert(selected.count == SynthPattern::kSteps);
  assertMatchesP1CProjection(pattern, settings, selected);
}

void testLegacyTieProjectionRemainsByteIdentical() {
  RuntimePatternEventBank bank{};
  SynthPattern pattern = emptyPattern();
  pattern.steps[15].note = 60;
  pattern.steps[15].timing = 23;
  pattern.steps[0].note = -2;
  pattern.steps[0].timing = 1;

  const auto settings = settingsFor(0);
  assert(bank.refresh(0, 1, 3, pattern, settings) ==
         PatternBankRefreshStatus::Ready);
  const auto& selected = bank.select(0, 1, 3);
  assert(selected.count == 1);
  assertMatchesP1CProjection(pattern, settings, selected);
  const uint32_t endSubtick =
      static_cast<uint32_t>(selected.events[0].startTick) * kSubticksPerTick +
      selected.events[0].durationSubticks;
  assert(endSubtick > static_cast<uint32_t>(kTicksPerBar) * kSubticksPerTick);
}

void testInvalidRefreshIsFailureAtomic() {
  RuntimePatternEventBank bank{};
  SynthPattern original = emptyPattern();
  original.steps[2].note = 52;
  assert(bank.refresh(0, 0, 4, original, settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  const RuntimePatternEventBuffer before = bank.select(0, 0, 4);

  SynthPattern replacement = emptyPattern();
  replacement.steps[5].note = 77;
  assert(bank.refresh(0, 0, 4, replacement, settingsFor(1)) ==
         PatternBankRefreshStatus::InvalidSettings);
  assert(std::memcmp(&before, &bank.select(0, 0, 4), sizeof(before)) == 0);

  assert(bank.refresh(2, 0, 4, replacement, settingsFor(0)) ==
         PatternBankRefreshStatus::InvalidAddress);
  assert(std::memcmp(&before, &bank.select(0, 0, 4), sizeof(before)) == 0);
}

void testInvalidSelectionAndNoPatternUseCanonicalSilence() {
  RuntimePatternEventBank bank{};
  const auto& empty = bank.empty();
  assert(empty.count == 0);
  assert(empty.lengthTicks() == kTicksPerBar);
  assert(&bank.select(2, 0, 0) == &empty);
  assert(&bank.select(0, kBankCount, 0) == &empty);
  assert(&bank.select(0, 0, Bank<SynthPattern>::kPatterns) == &empty);
}

void testRefreshingOneSlotDoesNotTouchNeighbors() {
  RuntimePatternEventBank bank{};
  SynthPattern left = emptyPattern();
  SynthPattern right = emptyPattern();
  left.steps[0].note = 40;
  right.steps[0].note = 41;
  assert(bank.refresh(0, 0, 0, left, settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.refresh(0, 0, 1, right, settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  const RuntimePatternEventBuffer neighborBefore = bank.select(0, 0, 1);

  left.steps[0].note = 72;
  assert(bank.refresh(0, 0, 0, left, settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.select(0, 0, 0).events[0].note == 72);
  assert(std::memcmp(&neighborBefore, &bank.select(0, 0, 1),
                     sizeof(neighborBefore)) == 0);
}

}  // namespace

int main() {
  testCompactCarrierBudgetAndAbi();
  testEveryResidentAddressIsIndependent();
  testCompactProjectionMatchesP1CExactly();
  testSixteenOnsetsFitWithoutPhraseCapacity();
  testLegacyTieProjectionRemainsByteIdentical();
  testInvalidRefreshIsFailureAtomic();
  testInvalidSelectionAndNoPatternUseCanonicalSilence();
  testRefreshingOneSlotDoesNotTouchNeighbors();
  std::puts("PATTERN/PHRASE P2 compact Pattern event bank: PASS");
  return 0;
}
