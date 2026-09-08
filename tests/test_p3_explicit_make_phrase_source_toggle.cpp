// PPC source-semantics RED.
//
// UI Constitution V1 freezes two independent operations:
//   1. source change does not implicitly convert material;
//   2. MAKE PHRASE remains explicit and one-way.
//
// The M4 base violates that contract in PhraseSourceToggle::toggle(): when a
// voice is on PATTERN and its editable event buffer is empty, the source toggle
// calls MiniAcid::makePhrase() and therefore creates material as a side effect.
//
// This test is intentionally behavioral. It does not grep for a function name
// or prescribe the future MAKE PHRASE key. It asks only that selecting a source
// must not perform the separate musical conversion action.

#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "PPC explicit MAKE PHRASE RED: %s\n", message);
  ++failures;
}
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};
  constexpr int kVoice = 0;
  constexpr int kOtherVoice = 1;

  // Deterministic Pattern A. The current buggy source-toggle path will have
  // something real to project, so a failure cannot be dismissed as an empty
  // default Pattern edge case.
  SynthPattern& pattern = engine.editSynthPattern(kVoice);
  pattern.steps[0].note = 36;
  pattern.steps[4].note = 43;
  pattern.steps[8].note = 41;
  pattern.steps[12].note = 48;

  const int notesBefore[4] = {
      pattern.steps[0].note,
      pattern.steps[4].note,
      pattern.steps[8].note,
      pattern.steps[12].note,
  };

  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "voice did not start on PATTERN");
  expect(engine.currentPhraseBuffer(kVoice).count == 0,
         "test precondition failed: editable event buffer was not empty");
  expect(engine.currentSequencedSource(kOtherVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "other voice did not start on PATTERN");

  PhraseSourceToggle::AudioGuard noGuard{};
  PhraseSourceToggle::toggle(engine, noGuard, kVoice);

  // Source selection is not MAKE PHRASE. With no independent Phrase/Melody
  // material available, the safe result is to stay on PATTERN rather than
  // synthesize a new owner behind the user's back.
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "SOURCE toggle implicitly converted PATTERN into editable material");
  expect(engine.currentPhraseBuffer(kVoice).count == 0,
         "SOURCE toggle created Phrase/Melody events without MAKE PHRASE");

  // Even the rejected source request must leave Pattern A byte-semantically
  // intact at the musical positions used by this scenario.
  const SynthPattern& after = engine.activeSynthPattern(kVoice);
  expect(after.steps[0].note == notesBefore[0] &&
             after.steps[4].note == notesBefore[1] &&
             after.steps[8].note == notesBefore[2] &&
             after.steps[12].note == notesBefore[3],
         "SOURCE toggle mutated Pattern material");

  expect(engine.currentSequencedSource(kOtherVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "source request leaked into the other synth voice");
  expect(engine.currentPhraseBuffer(kOtherVoice).count == 0,
         "source request wrote editable material into the other synth voice");

  if (failures == 0) {
    std::printf("PPC explicit MAKE PHRASE source separation: PASS\n");
    return 0;
  }

  std::fprintf(stderr,
               "PPC explicit MAKE PHRASE source separation: %d failure(s)\n",
               failures);
  return 1;
}
