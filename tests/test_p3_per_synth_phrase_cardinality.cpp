// P3 RED/GREEN #1B: per-synth sequenced source cardinality.
//
// Synth A and Synth B independently select PATTERN xor PHRASE and each own a
// bounded RuntimeSynthEventBuffer. Routing lives in the container, not in the
// event: RuntimeSynthEvent carries no target field and must not gain one.

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
  std::fprintf(stderr, "P3 per-synth cardinality FAIL: %s\n", message);
  ++g_failures;
}

constexpr int kSynthA = 0;
constexpr int kSynthB = 1;
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};

  // Every combination of per-synth source selection is representable.
  const MiniAcid::SequencedSource kPattern = MiniAcid::SequencedSource::Pattern;
  const MiniAcid::SequencedSource kPhrase = MiniAcid::SequencedSource::Phrase;
  const MiniAcid::SequencedSource combos[4][2] = {
      {kPattern, kPattern},
      {kPhrase, kPattern},
      {kPattern, kPhrase},
      {kPhrase, kPhrase},
  };
  for (const auto& combo : combos) {
    engine.setSequencedSource(kSynthA, combo[0]);
    engine.setSequencedSource(kSynthB, combo[1]);
    expect(engine.currentSequencedSource(kSynthA) == combo[0],
           "Synth A source did not hold its own selection");
    expect(engine.currentSequencedSource(kSynthB) == combo[1],
           "Synth B source did not hold its own selection");
  }

  // Per-synth phrase length is independent.
  expect(engine.setPhraseLength(kSynthA, 2), "Synth A length 2 rejected");
  expect(engine.setPhraseLength(kSynthB, 8), "Synth B length 8 rejected");
  expect(engine.currentPhraseBuffer(kSynthA).lengthTicks == 768,
         "Synth A phrase span is not 2 bars");
  expect(engine.currentPhraseBuffer(kSynthB).lengthTicks == 3072,
         "Synth B phrase span is not 8 bars");

  // Changing A must not disturb B.
  expect(engine.setPhraseLength(kSynthA, 4), "Synth A relength rejected");
  expect(engine.currentPhraseBuffer(kSynthA).lengthTicks == 1536,
         "Synth A relength did not apply");
  expect(engine.currentPhraseBuffer(kSynthB).lengthTicks == 3072,
         "changing Synth A length mutated Synth B");

  // Material written into A must not appear in B.
  engine.currentPhraseBuffer(kSynthA).events[0].note = 60;
  engine.currentPhraseBuffer(kSynthA).events[0].startTick = 360;
  engine.currentPhraseBuffer(kSynthA).events[0].durationSubticks =
      static_cast<uint16_t>(96 * PhraseRuntime::kSubticksPerTick);
  engine.currentPhraseBuffer(kSynthA).count = 1;

  expect(engine.currentPhraseBuffer(kSynthB).count == 0,
         "writing Synth A material leaked into Synth B");
  expect(engine.currentPhraseBuffer(kSynthB).events[0].note == 0,
         "Synth B event storage aliases Synth A");
  expect(&engine.currentPhraseBuffer(kSynthA) !=
             &engine.currentPhraseBuffer(kSynthB),
         "both synths share one phrase buffer");

  // Invalid length on one voice must not disturb either voice.
  const uint16_t keepA = engine.currentPhraseBuffer(kSynthA).lengthTicks;
  const uint16_t keepB = engine.currentPhraseBuffer(kSynthB).lengthTicks;
  for (const uint8_t bars : {0, 3, 5, 6, 7, 9, 16}) {
    expect(!engine.setPhraseLength(kSynthA, bars),
           "invalid per-synth length was accepted");
  }
  expect(engine.currentPhraseBuffer(kSynthA).lengthTicks == keepA,
         "rejected length mutated Synth A");
  expect(engine.currentPhraseBuffer(kSynthB).lengthTicks == keepB,
         "rejected length on A mutated Synth B");

  // The event ABI must stay routing-free.
  static_assert(sizeof(PhraseRuntime::RuntimeSynthEvent) == 10,
                "RuntimeSynthEvent must not grow a target field");
  static_assert(sizeof(PhraseRuntime::RuntimeSynthEventBuffer) == 1284,
                "phrase buffer footprint must stay fixed");

  if (g_failures != 0) {
    std::fprintf(stderr, "P3 per-synth cardinality: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("P3 per-synth phrase cardinality (real MiniAcid): OK\n");
  return 0;
}
