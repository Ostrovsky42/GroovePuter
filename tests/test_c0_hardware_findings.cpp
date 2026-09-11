#include <cassert>
#include <cstdint>

#include "src/ui/phrase_instrument_controls.h"
#include "src/phrase/runtime_phrase_generation.h"

namespace {

void testLengthRefusalPreservesMusicalReason() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar * 2u;
  phrase.count = 2;
  phrase.events[0].startTick = 0;
  phrase.events[0].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 48;
  phrase.events[1].startTick = PhraseRuntime::kTicksPerBar + 24u;
  phrase.events[1].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  phrase.events[1].note = 52;

  bool setterCalled = false;
  const auto outcome = PhraseInstrumentControls::applyLengthChangeDetailed(
      phrase, -1, [&](uint8_t) {
        setterCalled = true;
        return true;
      });

  assert(outcome.targetBars == 1);
  assert(outcome.result ==
         PhraseInstrumentControls::LengthChangeResult::WouldTruncateEvent);
  assert(!setterCalled);
  assert(phrase.lengthTicks == PhraseRuntime::kTicksPerBar * 2u);
  assert(phrase.count == 2);
  assert(phrase.events[1].startTick == PhraseRuntime::kTicksPerBar + 24u);
}

void testGeneratedPatternProjectsDirectlyIntoPhraseExtent() {
  SynthPattern generated{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    generated.steps[i].note = -1;
    generated.steps[i].accent = false;
    generated.steps[i].slide = false;
  }
  generated.steps[0].note = 48;
  generated.steps[0].accent = true;
  generated.steps[8].note = 52;

  const SynthPattern original = generated;
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = 0;
  settings.swingEnabled = false;
  settings.swingPercent = 50;
  settings.gateLengthRatio = 0.5f;

  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar * 2u;

  PhraseRuntime::RuntimeSynthEventBuffer candidate{};
  const auto result = PhraseGeneration::projectRepeatedPattern(
      generated, settings, phrase.lengthTicks, candidate);

  assert(result == PhraseGeneration::Result::Ready);
  assert(candidate.lengthTicks == phrase.lengthTicks);
  assert(candidate.count == 4);
  assert(candidate.events[0].startTick == 0);
  assert(candidate.events[1].startTick == PhraseRuntime::kTicksPerBar / 2u);
  assert(candidate.events[2].startTick == PhraseRuntime::kTicksPerBar);
  assert(candidate.events[3].startTick ==
         PhraseRuntime::kTicksPerBar + PhraseRuntime::kTicksPerBar / 2u);
  assert(candidate.events[0].note == 48);
  assert(candidate.events[2].note == 48);
  assert(candidate.events[0].flags & PhraseRuntime::kEventAccent);

  // The generator candidate is an input value, never a Pattern commit target.
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    assert(generated.steps[i].note == original.steps[i].note);
    assert(generated.steps[i].accent == original.steps[i].accent);
    assert(generated.steps[i].slide == original.steps[i].slide);
  }
}

}  // namespace

int main() {
  testLengthRefusalPreservesMusicalReason();
  testGeneratedPatternProjectsDirectlyIntoPhraseExtent();
  return 0;
}
