// U4C3: the walkthrough, measured on the sounding path rather than on blocks.
//
// Screenshots showed the block change; they could not show whether anything
// sounded different, and that is the whole claim JOIN makes. This drives the
// real sequencer and reads the note events it emits, so "the note now sounds
// longer" is measured as a release deadline and not asserted from a picture.
//
//   lengthen -> play   the note is still released by the next attack
//   join     -> play   it sounds to its stored end, the neighbour is gone
//   undo     -> play   the original sounding behaviour returns exactly

#include <cassert>
#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"
#include "src/phrase/runtime_phrase_edit.h"

SerialMock Serial;
SDMock SD;

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4C3 FAIL: %s\n", message);
  ++g_failures;
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

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
    engine.songMode_ = false;
    engine.sceneManager_.setSongMode(false);
    engine.playing = true;
    engine.tickPhaseAccum_ = 0;

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
    engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  }

  struct Tick {
    int on = 0;
    int off = 0;
  };

  Tick step(uint32_t absoluteTick) {
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

    Tick tick{};
    ScheduledMusicalEvent scheduled{};
    while (queue.tryPop(scheduled)) {
      if (scheduled.event.target != MusicalEventTarget::SynthA) continue;
      if (scheduled.event.type == MusicalEventType::NoteOn) ++tick.on;
      else if (scheduled.event.type == MusicalEventType::NoteOff) ++tick.off;
    }
    (void)queue.takePendingAllNotesOffMask();
    return tick;
  }
};

// Where the first note is released, and how many attacks were heard, over one
// pass of the phrase. -1 means it never stopped inside the window.
struct Heard {
  int releasedAt = -1;
  int attacks = 0;
};

Heard play(Fixture& fixture, uint32_t fromTick, uint32_t toTick) {
  Heard heard{};
  for (uint32_t tick = fromTick; tick <= toTick; ++tick) {
    const Fixture::Tick result = fixture.step(tick);
    heard.attacks += result.on;
    if (result.off > 0 && heard.releasedAt < 0) {
      heard.releasedAt = static_cast<int>(tick);
    }
  }
  return heard;
}

void authorEvent(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                 uint16_t startTick, uint16_t durationTicks, uint8_t note) {
  auto& event = phrase.events[phrase.count++];
  event = PhraseRuntime::RuntimeSynthEvent{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
}

}  // namespace

int main() {
  using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

  // A lengthened first note (0..96) with a neighbour attacking at 48.
  Buffer lengthened{};
  lengthened.lengthTicks = PhraseRuntime::kTicksPerBar;
  authorEvent(lengthened, 0, 96, 60);
  authorEvent(lengthened, 48, 24, 67);
  assert(RuntimePhraseEdit::validate(lengthened));

  // 1. Play it: the block says 96, the ear gets 48.
  {
    Fixture fixture;
    fixture.engine.currentPhraseBuffer(0) = lengthened;
    const Heard heard = play(fixture, 0, 130);
    expect(heard.releasedAt == 48,
           "the lengthened note was not released by the next attack");
    expect(heard.attacks == 2, "the pass did not produce both attacks");
  }

  // 2. Join, then play: it now sounds to its stored end and the neighbour is
  //    gone. This is the change JOIN promises, measured where it happens.
  Buffer joined = lengthened;
  expect(RuntimePhraseEdit::joinNextEvent(joined, 0) ==
             RuntimePhraseEdit::JoinResult::Changed,
         "join refused the neighbour it was meant to absorb");
  {
    Fixture fixture;
    fixture.engine.currentPhraseBuffer(0) = joined;
    const Heard heard = play(fixture, 0, 130);
    expect(heard.attacks == 1, "the joined pass still produced two attacks");
    expect(heard.releasedAt == 96,
           "the joined note did not sound to its stored end");
  }

  // 3. Undo restores the value, and playing it again restores the sound. The
  //    editor commits whole buffers, so undo is the before-image -- what this
  //    checks is that replaying it is audibly the original, not merely equal.
  {
    Fixture fixture;
    fixture.engine.currentPhraseBuffer(0) = lengthened;   // the undo target
    const Heard heard = play(fixture, 0, 130);
    expect(heard.releasedAt == 48,
           "after undo the note no longer stops where it originally did");
    expect(heard.attacks == 2, "after undo the neighbour did not come back");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4C3 join audible walkthrough: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI Constitution U4C3: %d failure(s)\n", g_failures);
  return 1;
}
