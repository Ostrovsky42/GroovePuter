// P3 GREEN #1: bounded Phrase source, verified against the shipped MiniAcid.
//
// Proves only that MiniAcid can retain a bounded 1/2/4/8-bar Phrase and select
// it as the exclusive sequenced source. Deliberately does NOT require Pattern
// and Phrase to expose the same concrete buffer type: they keep different
// retained representations by design.

#include <cstdint>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"
#include "src/phrase/runtime_synth_events.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "P3 phrase source FAIL: %s\n", message);
  ++g_failures;
}
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};

  // 1. Default sequenced source is Pattern.
  expect(engine.currentSequencedSource() == MiniAcid::SequencedSource::Pattern,
         "default sequenced source is not Pattern");

  // 2. Pattern -> Phrase -> Pattern round-trip.
  engine.setSequencedSource(MiniAcid::SequencedSource::Phrase);
  expect(engine.currentSequencedSource() == MiniAcid::SequencedSource::Phrase,
         "selecting Phrase did not take effect");
  engine.setSequencedSource(MiniAcid::SequencedSource::Pattern);
  expect(engine.currentSequencedSource() == MiniAcid::SequencedSource::Pattern,
         "returning to Pattern did not take effect");

  // 3. Valid lengths map to bar multiples of kTicksPerBar.
  for (const uint8_t bars : {1, 2, 4, 8}) {
    const uint16_t expected =
        static_cast<uint16_t>(bars * PhraseRuntime::kTicksPerBar);
    expect(engine.setPhraseLength(bars), "valid phrase length was rejected");
    expect(engine.currentPhraseBuffer().lengthTicks == expected,
           "valid phrase length produced the wrong tick span");
  }

  // 4. Invalid lengths are rejected without mutating the last valid state.
  expect(engine.setPhraseLength(4), "setup length 4 was rejected");
  const uint16_t retained = engine.currentPhraseBuffer().lengthTicks;
  for (const uint8_t bars : {0, 3, 5, 6, 7, 9, 16}) {
    expect(!engine.setPhraseLength(bars), "invalid phrase length was accepted");
    expect(engine.currentPhraseBuffer().lengthTicks == retained,
           "invalid phrase length mutated retained state");
  }

  // 5. Phrase material can be stored and read back.
  {
    PhraseRuntime::RuntimeSynthEventBuffer& phrase = engine.currentPhraseBuffer();
    phrase.events[0].note = 60;
    phrase.events[0].startTick = 360;
    phrase.events[0].durationSubticks =
        static_cast<uint16_t>(96 * PhraseRuntime::kSubticksPerTick);
    phrase.count = 1;
  }
  {
    const MiniAcid& constEngine = engine;
    const PhraseRuntime::RuntimeSynthEventBuffer& phrase =
        constEngine.currentPhraseBuffer();
    expect(phrase.count == 1, "phrase event count did not persist");
    expect(phrase.events[0].note == 60, "phrase event note did not persist");
    expect(phrase.events[0].startTick == 360,
           "phrase event startTick did not persist");
    expect(phrase.events[0].durationSubticks == 1536,
           "phrase event duration did not persist as subticks");
  }

  // 6. Capacity stays bounded by the existing runtime limits.
  static_assert(PhraseRuntime::kMaxSynthEvents == 128,
                "phrase capacity must remain the existing bound");
  static_assert(PhraseRuntime::kMaxPhraseBars == 8,
                "phrase bar bound must remain the existing bound");
  expect(engine.setPhraseLength(PhraseRuntime::kMaxPhraseBars),
         "maximum bar count was rejected");
  expect(engine.currentPhraseBuffer().lengthTicks == 3072,
         "maximum phrase span is not 8 bars");

  // 7. Source selection is one-valued: selecting one deselects the other.
  engine.setSequencedSource(MiniAcid::SequencedSource::Phrase);
  expect(engine.currentSequencedSource() != MiniAcid::SequencedSource::Pattern,
         "Phrase and Pattern were simultaneously selected");
  engine.setSequencedSource(MiniAcid::SequencedSource::Pattern);
  expect(engine.currentSequencedSource() != MiniAcid::SequencedSource::Phrase,
         "Pattern and Phrase were simultaneously selected");

  if (g_failures != 0) {
    std::fprintf(stderr, "P3 bounded phrase source: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("P3 bounded phrase source (real MiniAcid): OK\n");
  return 0;
}
