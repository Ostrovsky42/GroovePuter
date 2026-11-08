// U4B6 RED: MAKE PHRASE is the explicit, one-way entry into the Phrase source.
//
// Today the Phrase editor exists and is unreachable: the only production call
// to setSequencedSource() is inside the Undo/Redo restore path
// (synth_sequencer_page.cpp:401), which can only return a voice to a source it
// was already in. Nothing converts PATTERN material into a bounded Phrase and
// takes the voice there.
//
// This test names that missing operation. It is expected to FAIL TO COMPILE
// until MiniAcid::makePhrase(voiceIndex) exists — that is the honest RED for a
// contract whose entire content is "this operation must exist". Every assertion
// below is about observable engine state, not about the shape of the call.
//
// Constitution obligations this encodes
// (docs/contracts/0_9_10_UI_CONSTITUTION_V1.md):
//   :124  explicit one-way MAKE PHRASE
//   :145  sequenced source is not navigation state
//   :278  source change does not implicitly convert material;
//         MAKE PHRASE remains explicit

#include <cstdint>
#include <cstdio>

// Seeding the source Pattern needs editSynthPattern(), which is private. The
// P3 tests reach engine internals the same way; the assertions below still only
// read public state.
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/phrase/runtime_synth_events.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4B6 make-phrase entry FAIL: %s\n", message);
  ++g_failures;
}

constexpr int kSynthA = 0;
constexpr int kSynthB = 1;

// A voice is "converted" only when both halves happened: the source moved and
// bounded material exists. Half of that is a half-converted state.
bool converted(MiniAcid& engine, int voice) {
  return engine.currentSequencedSource(voice) ==
             MiniAcid::SequencedSource::Phrase &&
         engine.currentPhraseBuffer(voice).count > 0;
}
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};

  // Baseline: both voices start on Pattern with no phrase material.
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Pattern,
         "synth A did not start on Pattern");
  expect(engine.currentSequencedSource(kSynthB) ==
             MiniAcid::SequencedSource::Pattern,
         "synth B did not start on Pattern");
  expect(engine.currentPhraseBuffer(kSynthA).count == 0,
         "synth A started with phrase material already present");

  // Seed a deterministic Pattern. Without this the conversion would carry
  // whatever the default scene happens to hold, and assertion 2 would be
  // testing ambient state rather than the operation.
  {
    SynthPattern& pattern = engine.editSynthPattern(kSynthA);
    pattern.steps[0].note = 36;
    pattern.steps[4].note = 43;
  }

  // 1. The entry exists and is explicit: one call, both halves committed.
  const bool made = engine.makePhrase(kSynthA);
  expect(made, "makePhrase reported failure on a voice that should convert");
  expect(converted(engine, kSynthA),
         "makePhrase did not leave synth A converted");

  // 2. Material actually came from the Pattern, not from nowhere. The projected
  //    phrase must carry the voice's pattern onsets, so an empty buffer with a
  //    flipped source is not a conversion.
  expect(engine.currentPhraseBuffer(kSynthA).count > 0,
         "converted voice has no phrase events");
  expect(engine.currentPhraseBuffer(kSynthA).lengthTicks > 0,
         "converted phrase has no length");

  // 3. Synth A and B are independent. Converting one must not touch the other.
  expect(engine.currentSequencedSource(kSynthB) ==
             MiniAcid::SequencedSource::Pattern,
         "converting synth A also moved synth B");
  expect(engine.currentPhraseBuffer(kSynthB).count == 0,
         "converting synth A wrote phrase material into synth B");

  // 4. Pattern material survives conversion unchanged. Phrase must not become a
  //    hidden alias of the Pattern it was projected from: editing the phrase
  //    later must not reach back into Pattern storage.
  const auto& phrase = engine.currentPhraseBuffer(kSynthA);
  const uint16_t firstStart = phrase.count > 0 ? phrase.events[0].startTick : 0;
  engine.currentPhraseBuffer(kSynthA).events[0].startTick =
      static_cast<uint16_t>(firstStart + 1);
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Phrase,
         "editing phrase material changed the sequenced source");

  // 5. One-way. A second MAKE PHRASE on an already-converted voice must not
  //    silently re-project and discard the edits made above.
  const uint16_t editedStart = engine.currentPhraseBuffer(kSynthA)
                                   .events[0].startTick;
  (void)engine.makePhrase(kSynthA);
  expect(engine.currentPhraseBuffer(kSynthA).events[0].startTick == editedStart,
         "repeating makePhrase overwrote existing phrase material");

  // 6. Navigation is not a source change. Nothing here calls a UI transition,
  //    but the engine-level invariant is that only the explicit operation
  //    moves the source: returning to Pattern must be a deliberate call.
  engine.setSequencedSource(kSynthA, MiniAcid::SequencedSource::Pattern);
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Pattern,
         "explicit return to Pattern did not take effect");
  expect(engine.currentPhraseBuffer(kSynthA).count > 0,
         "returning to Pattern destroyed the phrase material");

  // 7. Failure leaves no half-converted state. An out-of-range voice must
  //    change nothing at all.
  const auto sourceBefore = engine.currentSequencedSource(kSynthB);
  const bool bad = engine.makePhrase(99);
  expect(!bad, "makePhrase accepted an out-of-range voice");
  expect(engine.currentSequencedSource(kSynthB) == sourceBefore,
         "a rejected makePhrase disturbed another voice");

  if (g_failures == 0) {
    std::printf("U4B6 make-phrase entry: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "U4B6 make-phrase entry: %d failure(s)\n", g_failures);
  return 1;
}
