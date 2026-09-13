// C9 hardware regression: MAKE PHRASE must adapt cyclic Pattern lifetime to
// bounded linear Melody lifetime without changing Pattern projection semantics.

#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_synth_events.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "C9 make-phrase boundary FAIL: %s\n", message);
  ++g_failures;
}

void clearPattern(SynthPattern& pattern) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i] = SynthStep{};
    pattern.steps[i].note = -1;
    pattern.steps[i].probability = 100;
  }
}
}  // namespace

int main() {
  constexpr int kSynthA = 0;
  MiniAcid engine{44100.0f, nullptr};

  SynthPattern& pattern = engine.editSynthPattern(kSynthA);
  clearPattern(pattern);

  // This is the already-ratified cyclic Pattern case: a late onset plus a TIE
  // into step 0 produces a note whose physical lifetime crosses the bar end.
  pattern.steps[15].note = 60;
  pattern.steps[15].timing = 23;
  pattern.steps[0].note = -2;
  pattern.steps[0].timing = 1;

  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = 0;
  settings.gateLengthRatio = 0.8f;

  PhraseRuntime::RuntimeSynthEventBuffer cyclic{};
  expect(PhraseRuntime::projectPatternToRuntimeEvents(pattern, settings, cyclic) ==
             PhraseRuntime::PatternProjectionStatus::Ready,
         "cyclic Pattern projection was not ready");
  expect(cyclic.count > 0, "cyclic Pattern projection was empty");

  bool sawCrossBoundary = false;
  const uint32_t barEnd = static_cast<uint32_t>(PhraseRuntime::kTicksPerBar) *
                          PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < cyclic.count; ++i) {
    const auto& event = cyclic.events[i];
    const uint32_t end = static_cast<uint32_t>(event.startTick) *
                             PhraseRuntime::kSubticksPerTick +
                         event.durationSubticks;
    if (end > barEnd) sawCrossBoundary = true;
  }
  expect(sawCrossBoundary,
         "fixture no longer proves accepted cyclic cross-bar Pattern lifetime");
  expect(!RuntimePhraseEdit::validate(cyclic),
         "cyclic fixture unexpectedly already satisfies linear Melody bounds");

  const bool made = engine.makePhrase(kSynthA);
  expect(made, "MAKE PHRASE refused a valid cyclic Pattern");
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Phrase,
         "MAKE PHRASE did not publish Phrase source");

  const auto& melody = engine.currentPhraseBuffer(kSynthA);
  expect(RuntimePhraseEdit::validate(melody),
         "MAKE PHRASE published a Melody that its own editor rejects");
  expect(melody.lengthTicks == PhraseRuntime::kTicksPerBar,
         "MAKE PHRASE changed the one-bar material extent");

  const uint32_t melodyEnd = static_cast<uint32_t>(melody.lengthTicks) *
                             PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < melody.count; ++i) {
    const auto& event = melody.events[i];
    const uint32_t start = static_cast<uint32_t>(event.startTick) *
                           PhraseRuntime::kSubticksPerTick;
    const uint32_t end = start + event.durationSubticks;
    expect(event.durationSubticks > 0,
           "linearized Melody contains zero-duration event");
    expect(end <= melodyEnd,
           "linearized Melody still crosses its terminal boundary");
  }

  if (g_failures == 0) {
    std::printf("C9 make-phrase linear boundary: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "C9 make-phrase linear boundary: %d failure(s)\n",
               g_failures);
  return 1;
}
