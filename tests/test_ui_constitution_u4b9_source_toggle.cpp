// U4B9: one owner for the PATTERN/PHRASE switch, reachable by a key.
//
// The switch existed only as the SRC row on the MORE tab, three keypresses and
// a tab away from the editor it governs. Adding a hotkey without extracting the
// dispatch would leave two copies of a decision with three branches and an undo
// receipt -- the kind of pair that silently diverges.
//
// So the behaviour moves into PhraseSourceToggle and both callers use it. The
// contract tested here is the one the U4B6 entry established, now stated once:
//
//   PATTERN with no material  -> convert (makePhrase) and go to PHRASE
//   PATTERN with material     -> switch only; never re-project over edits
//   PHRASE                    -> back to PATTERN, keeping the material

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

  // 1. First use converts: material appears and the voice moves to PHRASE.
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Pattern,
         "voice did not start on PATTERN");
  PhraseSourceToggle::toggle(engine, nullptr, kSynthA);
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Phrase,
         "first toggle did not reach PHRASE");
  expect(engine.currentPhraseBuffer(kSynthA).count > 0,
         "first toggle produced no material");

  // 2. Editing the material and toggling twice must return it untouched. This
  //    is the invariant that makes the switch safe to use casually: it is a
  //    source change, not a conversion, once material exists.
  const uint16_t edited = static_cast<uint16_t>(
      engine.currentPhraseBuffer(kSynthA).events[0].startTick + 1u);
  engine.currentPhraseBuffer(kSynthA).events[0].startTick = edited;

  PhraseSourceToggle::toggle(engine, nullptr, kSynthA);
  expect(engine.currentSequencedSource(kSynthA) ==
             MiniAcid::SequencedSource::Pattern,
         "toggle did not return to PATTERN");
  expect(engine.currentPhraseBuffer(kSynthA).count > 0,
         "returning to PATTERN destroyed the material");

  PhraseSourceToggle::toggle(engine, nullptr, kSynthA);
  expect(engine.currentPhraseBuffer(kSynthA).events[0].startTick == edited,
         "toggling back re-projected over existing edits");

  // 3. Voices are independent.
  expect(engine.currentSequencedSource(kSynthB) ==
             MiniAcid::SequencedSource::Pattern,
         "toggling synth A moved synth B");
  expect(engine.currentPhraseBuffer(kSynthB).count == 0,
         "toggling synth A wrote material into synth B");

  // 4. An out-of-range voice changes nothing at all.
  const auto sourceBefore = engine.currentSequencedSource(kSynthA);
  PhraseSourceToggle::toggle(engine, nullptr, 9);
  expect(engine.currentSequencedSource(kSynthA) == sourceBefore,
         "a rejected toggle disturbed a real voice");

  // 5. The audio guard is honoured when supplied: the mutation must run inside
  //    it, not beside it.
  {
    int guardCalls = 0;
    bool ranInside = false;
    auto guard = [&](const std::function<void()>& body) {
      ++guardCalls;
      body();
      ranInside = true;
    };
    const auto before = engine.currentSequencedSource(kSynthB);
    PhraseSourceToggle::toggle(engine, guard, kSynthB);
    expect(guardCalls == 1, "the audio guard was not used");
    expect(ranInside, "the mutation did not run inside the guard");
    expect(engine.currentSequencedSource(kSynthB) != before,
         "the guarded toggle did not change the source");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4B9 source toggle: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI Constitution U4B9 source toggle: %d failure(s)\n",
               g_failures);
  return 1;
}
