// PAIR MELODY: MAKE MELODY with STEPS pair play N:N+1 projects a two-bar
// melody (bar 1 = slot N, bar 2 = slot N+1), pauses the pair while the voice
// plays the melody, and resumes the pair when the voice returns to STEPS.

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
  std::fprintf(stderr, "PAIR MELODY FAIL: %s\n", message);
  ++g_failures;
}

void clearPattern(SynthPattern& pattern) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i] = SynthStep{};
    pattern.steps[i].note = -1;
    pattern.steps[i].probability = 100;
  }
}

// One note on step 0 of the given slot, written through the current selector.
void writeSlot(MiniAcid& engine, int slot, int note) {
  engine.set303PatternIndex(0, slot);
  SynthPattern& pattern = engine.editSynthPattern(0);
  clearPattern(pattern);
  pattern.steps[0].note = note;
  pattern.steps[0].timing = 0;
}

// Sets up slot 0 (note 60) and slot 1 (note 67) and starts the pair on slot 0.
bool startPair(MiniAcid& engine, bool slotOneHasNote) {
  writeSlot(engine, 1, slotOneHasNote ? 67 : -1);
  writeSlot(engine, 0, 60);
  engine.set303PatternIndex(0, 0);
  return engine.togglePairPlay(0) == MiniAcid::PairPlayResult::Started;
}
}  // namespace

int main() {
  // 1. Pair on: the melody is two bars, slot N then slot N+1, and the pair pauses.
  {
    MiniAcid engine{44100.0f, nullptr};
    engine.init();
    engine.setSongMode(false);
    (void)engine.rebuildPatternRuntimeEventBank();
    expect(startPair(engine, true), "pair start on slot 0");

    expect(engine.makePhrase(0), "pair MAKE MELODY accepted");
    expect(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase,
           "voice 0 is on the melody");
    const auto* melody = engine.workingMaterial_[0].melodyIfHeld();
    expect(melody != nullptr, "melody is held");
    if (melody != nullptr) {
      expect(melody->lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
             "pair melody is two bars long");
      expect(melody->count == 2, "pair melody has one onset per bar");
      expect(melody->events[0].startTick == 0 && melody->events[0].note == 60,
             "bar 1 is slot N (note 60)");
      expect(melody->events[1].startTick == PhraseRuntime::kTicksPerBar &&
                 melody->events[1].note == 67,
             "bar 2 is slot N+1 (note 67)");
    }
    expect(engine.pairPlayActive(0), "pair stays registered while paused");
    expect(engine.pairPlay_[0].paused, "pair is paused by the melody");

    // A paused pair does not flip while the voice plays the melody.
    engine.start();
    engine.processSequencerEvents(0);
    engine.processSequencerEvents(384u);
    engine.processSequencerEvents(768u);
    expect(engine.pairPlayPhase(0) == 0, "paused pair does not flip");
  }

  // 2. Pair started on slot N+1 (phase 1): the melody still starts from slot N.
  {
    MiniAcid engine{44100.0f, nullptr};
    engine.init();
    engine.setSongMode(false);
    (void)engine.rebuildPatternRuntimeEventBank();
    expect(startPair(engine, true), "phase test: pair start");
    engine.start();
    engine.processSequencerEvents(0);     // armed start: bar 1 = N
    engine.processSequencerEvents(384u);  // flip: phase 1
    expect(engine.pairPlayPhase(0) == 1, "phase test: pair is on N+1");

    expect(engine.makePhrase(0), "phase test: MAKE MELODY accepted");
    const auto* melody = engine.workingMaterial_[0].melodyIfHeld();
    expect(melody != nullptr && melody->events[0].note == 60,
           "phase test: bar 1 still comes from slot N");
    expect(melody != nullptr && melody->events[1].note == 67,
           "phase test: bar 2 comes from slot N+1");
  }

  // 3. Slot N+1 empty: bar 2 is silent but the melody is still two bars.
  {
    MiniAcid engine{44100.0f, nullptr};
    engine.init();
    engine.setSongMode(false);
    (void)engine.rebuildPatternRuntimeEventBank();
    expect(startPair(engine, false), "empty N+1: pair start");
    expect(engine.makePhrase(0), "empty N+1: MAKE MELODY accepted");
    const auto* melody = engine.workingMaterial_[0].melodyIfHeld();
    expect(melody != nullptr && melody->lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
           "empty N+1: melody is still two bars");
    expect(melody != nullptr && melody->count == 1,
           "empty N+1: only bar 1 has an onset");
  }

  // 4. Back on STEPS the pair resumes from slot N.
  {
    MiniAcid engine{44100.0f, nullptr};
    engine.init();
    engine.setSongMode(false);
    (void)engine.rebuildPatternRuntimeEventBank();
    expect(startPair(engine, true), "resume: pair start");
    expect(engine.makePhrase(0), "resume: MAKE MELODY accepted");
    engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
    expect(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern,
           "resume: voice is back on STEPS");
    expect(engine.pairPlayActive(0) && !engine.pairPlay_[0].paused,
           "resume: pair is unpaused");
    expect(engine.pairPlayBaseSlot(0) == 0 && engine.pairPlayPhase(0) == 0,
           "resume: pair restarts from slot N");
  }

  // 5. Without a pair MAKE MELODY is unchanged: one bar, no pause.
  {
    MiniAcid engine{44100.0f, nullptr};
    engine.init();
    engine.setSongMode(false);
    (void)engine.rebuildPatternRuntimeEventBank();
    writeSlot(engine, 0, 60);
    expect(engine.makePhrase(0), "single: MAKE MELODY accepted");
    const auto* melody = engine.workingMaterial_[0].melodyIfHeld();
    expect(melody != nullptr && melody->lengthTicks == PhraseRuntime::kTicksPerBar,
           "single: melody is one bar");
    expect(!engine.pairPlayActive(0), "single: no pair is created");
  }

  if (g_failures != 0) return 1;
  std::puts("PAIR MELODY PASS");
  return 0;
}
