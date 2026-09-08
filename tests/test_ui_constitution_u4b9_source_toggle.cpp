// U4B9 / #444: SELECT SOURCE and MAKE PHRASE are distinct musical actions.
//
// Explicit PATTERN -> PHRASE source selection must never project Pattern
// material as a side effect. MAKE PHRASE is the one-way materialization
// command. Once material exists, source switching preserves it unchanged.

#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4B9 FAIL: %s\n", message);
  ++g_failures;
}

constexpr int kSynthA = 0;
constexpr int kSynthB = 1;
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};

  {
    SynthPattern& pattern = engine.editSynthPattern(kSynthA);
    pattern.steps[0].note = 36;
    pattern.steps[4].note = 43;
  }

  // 1. SOURCE is selection only. An empty Phrase stays empty.
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Pattern,
         "voice did not start on PATTERN");
  expect(engine.currentPhraseBuffer(kSynthA).count == 0,
         "fixture unexpectedly started with Phrase material");
  const auto selected = PhraseSourceToggle::toggle(engine, nullptr, kSynthA);
  expect(selected == PhraseSourceToggle::Result::SwitchedToPhrase,
         "explicit SOURCE did not report a pure switch to PHRASE");
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Phrase,
         "explicit SOURCE did not reach PHRASE");
  expect(engine.currentPhraseBuffer(kSynthA).count == 0,
         "explicit SOURCE silently materialized Pattern as Phrase");

  // 2. Return to PATTERN, then MAKE PHRASE is the distinct materialization.
  expect(PhraseSourceToggle::toggle(engine, nullptr, kSynthA) ==
             PhraseSourceToggle::Result::SwitchedToPattern,
         "SOURCE did not return to PATTERN");
  expect(PhraseSourceToggle::makePhrase(engine, nullptr, kSynthA),
         "explicit MAKE PHRASE failed");
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Phrase,
         "MAKE PHRASE did not select PHRASE after materialization");
  expect(engine.currentPhraseBuffer(kSynthA).count > 0,
         "MAKE PHRASE produced no material");

  // 3. Existing Phrase edits survive pure source changes.
  const uint16_t edited = static_cast<uint16_t>(
      engine.currentPhraseBuffer(kSynthA).events[0].startTick + 1u);
  engine.currentPhraseBuffer(kSynthA).events[0].startTick = edited;

  PhraseSourceToggle::toggle(engine, nullptr, kSynthA);
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Pattern,
         "SOURCE did not return to PATTERN after editing");
  PhraseSourceToggle::toggle(engine, nullptr, kSynthA);
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Phrase,
         "SOURCE did not return to PHRASE after editing");
  expect(engine.currentPhraseBuffer(kSynthA).events[0].startTick == edited,
         "SOURCE re-projected over existing Phrase edits");

  // 4. Voices are independent.
  expect(engine.currentSequencedSource(kSynthB) ==
             MiniAcid::SequencedSource::Pattern,
         "synth A actions moved synth B source");
  expect(engine.currentPhraseBuffer(kSynthB).count == 0,
         "synth A actions wrote material into synth B");

  // 5. Invalid voice is rejected without disturbing a real voice.
  const auto sourceBefore = engine.currentSequencedSource(kSynthA);
  expect(PhraseSourceToggle::toggle(engine, nullptr, 9) ==
             PhraseSourceToggle::Result::Rejected,
         "invalid voice was not rejected");
  expect(engine.currentSequencedSource(kSynthA) == sourceBefore,
         "rejected SOURCE disturbed a real voice");

  // 6. The audio guard wraps source mutation.
  {
    int guardCalls = 0;
    bool ranInside = false;
    auto guard = [&](const std::function<void()>& body) {
      ++guardCalls;
      body();
      ranInside = true;
    };
    const auto before = engine.currentSequencedSource(kSynthB);
    const auto result = PhraseSourceToggle::toggle(engine, guard, kSynthB);
    expect(result == PhraseSourceToggle::Result::SwitchedToPhrase,
           "guarded empty-Phrase SOURCE was not a pure source switch");
    expect(guardCalls == 1, "the audio guard was not used");
    expect(ranInside, "the source mutation did not run inside the guard");
    expect(engine.currentSequencedSource(kSynthB) != before,
           "guarded SOURCE did not change source");
    expect(engine.currentPhraseBuffer(kSynthB).count == 0,
           "guarded SOURCE materialized an empty Phrase");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4B9 source vs MAKE PHRASE: PASS\n");
    return 0;
  }
  std::fprintf(stderr,
               "UI Constitution U4B9 source vs MAKE PHRASE: %d failure(s)\n",
               g_failures);
  return 1;
}
