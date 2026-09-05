// P3 RED/GREEN #2: phrase-relative onset addressing.
//
// A Phrase is 1/2/4/8 bars long, so its onsets must be addressed in
// phrase-relative time, absoluteTick % lengthTicks, and not in the bar-local
// time Pattern uses (absoluteTick % 384). Two failures follow from bar-local
// addressing and this test pins both:
//
//   - an event at startTick 360 in a 768-tick phrase repeats at absolute 744,
//     because 744 % 384 == 360;
//   - an event at startTick >= 384 is unreachable, because barTick <= 383.
//
// Cross-bar note lifetime is NOT under test here: RuntimeSynthPlaybackState
// already keeps release deadlines in absolute subticks, and
// testOrdinaryBarWrapIsNotABarrier() in the P2 suite already proves a note
// survives a bar boundary.

#include <cassert>
#include <cstdint>
#include <cstdio>

// Host-test only: reach the tick seam and the playback state the way the
// existing P0/P2 runtime characterizations do, instead of adding a production probe.
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"

SerialMock Serial;
SDMock SD;

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "P3 phrase-relative onset FAIL: %s\n", message);
  ++g_failures;
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

// What a single tick produced. MusicalEvent carries no timestamp, so the tick a
// trace belongs to is established by draining once per tick.
struct TickTrace {
  int noteOn[NUM_303_VOICES]{};
  int noteOff[NUM_303_VOICES]{};
};

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};
  uint32_t blockSequence{1};

  Fixture() {
    engine.setBpm(120.0f);
    assert(engine.rebuildPatternRuntimeEventBank());
    engine.setCurrentPage(
        static_cast<int8_t>(PatternPagingService::activePageIndex()));
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);

    // No SONG playhead side effects at bar boundaries, and no start() so that
    // currentTick_ stays under this test's control.
    engine.songMode_ = false;
    engine.sceneManager_.setSongMode(false);
    engine.playing = true;
    engine.tickPhaseAccum_ = 0;  // subtick == tick * 16 exactly
  }

  // Mirrors production order within one sample: advance the tick (onsets), then
  // evaluate release deadlines. processSequencerEvents() does onsets only;
  // releaseDue() normally runs in the per-sample loop of generateAudioBuffer().
  TickTrace step(uint32_t absoluteTick) {
    queue.beginMidiRenderBlock(blockSequence++, 512,
                               engine.transportPhaseSteps(), engine.bpm(),
                               engine.sampleRate(), engine.isPlaying(),
                               false, false);
    engine.currentTick_ = absoluteTick;
    engine.processSequencerEvents(absoluteTick);
    for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
      engine.consumePatternPlaybackActions_(
          voice, engine.patternPlaybackState_[voice].releaseDue(
                     absoluteTick * PhraseRuntime::kSubticksPerTick));
    }
    queue.endMidiRenderBlock();

    TickTrace trace{};
    ScheduledMusicalEvent scheduled{};
    while (queue.tryPop(scheduled)) {
      int voice = -1;
      if (scheduled.event.target == MusicalEventTarget::SynthA) voice = 0;
      else if (scheduled.event.target == MusicalEventTarget::SynthB) voice = 1;
      if (voice < 0) continue;
      if (scheduled.event.type == MusicalEventType::NoteOn) {
        ++trace.noteOn[voice];
      } else if (scheduled.event.type == MusicalEventType::NoteOff) {
        ++trace.noteOff[voice];
      }
    }
    (void)queue.takePendingAllNotesOffMask();
    return trace;
  }

  // Empty Pattern material on both voices, so any onset observed while a voice
  // is on PHRASE must have come from phrase material.
  void silencePatterns() {
    for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
      engine.set303PatternIndex(voice, 0);
      SynthPattern& pattern = engine.editSynthPattern(voice);
      pattern = SynthPattern{};
      for (int step = 0; step < SynthPattern::kSteps; ++step) {
        pattern.steps[step].note = -1;
        pattern.steps[step].timing = 0;
        pattern.steps[step].probability = 100;
        pattern.steps[step].ghost = false;
      }
      assert(engine.refreshPatternRuntimeEvents(
          voice, engine.current303BankIndex(voice), 0));
    }
  }
};

// probability 100 and no ghost flag take the zero-RNG-draw path in
// triggerSynthStep_, so the test is immune to the global rand() stream.
void authorEvent(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                 uint16_t startTick,
                 uint8_t note,
                 uint16_t durationSubticks) {
  PhraseRuntime::RuntimeSynthEvent& event = phrase.events[phrase.count++];
  event.startTick = startTick;
  event.durationSubticks = durationSubticks;
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
  event.flags = 0;
  event.fx = 0;
  event.fxParam = 0;
}

constexpr int kA = 0;
constexpr int kB = 1;

void expectTick(Fixture& f, uint32_t tick, int voice,
                int wantOn, int wantOff, const char* what) {
  const TickTrace trace = f.step(tick);
  char message[192];
  std::snprintf(message, sizeof(message),
                "tick %u %s: expected %d on / %d off, got %d on / %d off",
                tick, what, wantOn, wantOff,
                trace.noteOn[voice], trace.noteOff[voice]);
  expect(trace.noteOn[voice] == wantOn && trace.noteOff[voice] == wantOff,
         message);
}

// Synth A on a 2-bar phrase: the canonical loop and reachability proof.
void testTwoBarPhraseAddressing() {
  Fixture f;
  f.silencePatterns();

  expect(f.engine.setPhraseLength(kA, 2), "2-bar phrase length rejected");
  PhraseRuntime::RuntimeSynthEventBuffer& phrase = f.engine.currentPhraseBuffer(kA);
  phrase.count = 0;
  authorEvent(phrase, 360, 60, 96 * PhraseRuntime::kSubticksPerTick);  // 1536
  authorEvent(phrase, 500, 62, 6 * PhraseRuntime::kSubticksPerTick);
  f.engine.setSequencedSource(kA, MiniAcid::SequencedSource::Phrase);

  expectTick(f, 360, kA, 1, 0, "first onset of event A");
  expect(f.engine.patternPlaybackState_[kA].releaseAtSubtick() ==
             456u * PhraseRuntime::kSubticksPerTick,
         "release deadline for event A is not tick 456");

  expectTick(f, 384, kA, 0, 0, "bar boundary must not start or release");
  expect(f.engine.patternPlaybackState_[kA].active(),
         "event A stopped being active at the bar boundary");

  expectTick(f, 456, kA, 0, 1, "release of event A");

  // 500 is unreachable in bar-local time: barTick never exceeds 383.
  expectTick(f, 500, kA, 1, 0, "event B in the second bar");
  expectTick(f, 506, kA, 0, 1, "release of event B");

  // 744 % 384 == 360, so bar-local addressing repeats event A here.
  expectTick(f, 744, kA, 0, 0, "event A must not repeat at bar-local 360");

  // 884 matches neither event under either addressing.
  expectTick(f, 884, kA, 0, 0, "no event at 884");

  // 1128 % 768 == 360: the genuine loop point of the phrase.
  expectTick(f, 1128, kA, 1, 0, "event A repeats at the phrase loop point");
  expectTick(f, 1224, kA, 0, 1, "release of looped event A");

  // 1268 % 768 == 500, while 1268 % 384 == 116.
  expectTick(f, 1268, kA, 1, 0, "event B repeats at the phrase loop point");
}

// A and B on different phrase lengths at the same time: addressing must read
// each voice's own lengthTicks.
void testPerVoicePhraseLengthsAreIndependent() {
  Fixture f;
  f.silencePatterns();

  expect(f.engine.setPhraseLength(kA, 2), "voice A 2-bar length rejected");
  expect(f.engine.setPhraseLength(kB, 8), "voice B 8-bar length rejected");

  PhraseRuntime::RuntimeSynthEventBuffer& phraseA = f.engine.currentPhraseBuffer(kA);
  phraseA.count = 0;
  authorEvent(phraseA, 360, 60, 6 * PhraseRuntime::kSubticksPerTick);

  PhraseRuntime::RuntimeSynthEventBuffer& phraseB = f.engine.currentPhraseBuffer(kB);
  phraseB.count = 0;
  authorEvent(phraseB, 1000, 64, 6 * PhraseRuntime::kSubticksPerTick);

  f.engine.setSequencedSource(kA, MiniAcid::SequencedSource::Phrase);
  f.engine.setSequencedSource(kB, MiniAcid::SequencedSource::Phrase);

  // 1000 % 3072 == 1000 fires B; 1000 % 768 == 232 leaves A silent.
  const TickTrace atThousand = f.step(1000);
  expect(atThousand.noteOn[kB] == 1,
         "voice B event at 1000 did not fire under its own 8-bar length");
  expect(atThousand.noteOn[kA] == 0,
         "voice A fired at 1000, so addressing is not per voice");
}

// A stays on PATTERN while B is on PHRASE: the Pattern path must be untouched.
void testPatternVoiceIsUnaffectedByPhraseVoice() {
  Fixture f;
  f.silencePatterns();

  // Step 15 of an otherwise empty pattern lands on barTick 360.
  f.engine.set303PatternIndex(kA, 0);
  SynthPattern& pattern = f.engine.editSynthPattern(kA);
  pattern.steps[15].note = 24;
  pattern.steps[15].velocity = 100;
  pattern.steps[15].probability = 100;
  pattern.steps[15].ghost = false;
  pattern.steps[15].slide = false;
  pattern.steps[15].accent = false;
  assert(f.engine.refreshPatternRuntimeEvents(
      kA, f.engine.current303BankIndex(kA), 0));

  expect(f.engine.setPhraseLength(kB, 2), "voice B 2-bar length rejected");
  PhraseRuntime::RuntimeSynthEventBuffer& phraseB = f.engine.currentPhraseBuffer(kB);
  phraseB.count = 0;
  authorEvent(phraseB, 500, 64, 6 * PhraseRuntime::kSubticksPerTick);
  f.engine.setSequencedSource(kB, MiniAcid::SequencedSource::Phrase);

  // Pattern voice A keeps firing once per bar at bar-local 360. Only onsets are
  // asserted: because this test visits ticks 360 and 744 and skips everything
  // between, the release of the 360 note surfaces at 744, the first visited tick
  // past its deadline. That is an artifact of tick injection, not of ordering.
  const TickTrace atThreeSixty = f.step(360);
  expect(atThreeSixty.noteOn[kA] == 1, "PATTERN voice A did not fire at 360");
  const TickTrace atSevenFortyFour = f.step(744);
  expect(atSevenFortyFour.noteOn[kA] == 1,
         "PATTERN voice A stopped repeating every bar");
}

}  // namespace

int main() {
  testTwoBarPhraseAddressing();
  testPerVoicePhraseLengthsAreIndependent();
  testPatternVoiceIsUnaffectedByPhraseVoice();

  if (g_failures != 0) {
    std::fprintf(stderr, "P3 phrase-relative onset: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("P3 phrase-relative onset addressing: OK\n");
  return 0;
}
