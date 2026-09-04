#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/phrase/runtime_synth_events.h"

int main() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
  }

  // The first note has no reachable TIE before the conditional onset. Its
  // natural lifetime ends before tick 48. A rejected conditional onset at
  // tick 48 must not allow the later TIE at tick 72 to resurrect it.
  pattern.steps[0].note = 60;
  pattern.steps[0].probability = 100;
  pattern.steps[2].note = 64;
  pattern.steps[2].probability = 0;
  pattern.steps[3].note = -2;

  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = 0;
  settings.swingPercent = 50;
  settings.swingEnabled = false;
  settings.gateLengthRatio = 1.0f;

  PhraseRuntime::RuntimeSynthEventBuffer projected{};
  assert(PhraseRuntime::projectPatternToRuntimeEvents(
             pattern, settings, projected) ==
         PhraseRuntime::PatternProjectionStatus::Ready);
  assert(projected.count == 2);

  const auto& first = projected.events[0];
  const auto& conditional = projected.events[1];
  const uint32_t conditionalStart =
      static_cast<uint32_t>(conditional.startTick) *
      PhraseRuntime::kSubticksPerTick;
  const uint32_t laterTieStart = 72u * PhraseRuntime::kSubticksPerTick;

  assert(first.durationSubticks < conditionalStart);
  assert(first.durationSubticks < laterTieStart);
  std::puts("PATTERN/PHRASE P2 conditional expiry no-resurrection: PASS");
  return 0;
}
