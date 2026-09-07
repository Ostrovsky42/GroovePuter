// M3: one resolved authority per voice, published from the control side and
// only read in the audio path.
//
// This is the first checkpoint where a mistake stops meaning "the wrong file
// was saved" and starts meaning "the wrong material is actually sounding". So
// the audio path is not allowed to work anything out: no Scene, no descriptor,
// no filesystem identity, no Song resolution. It reads a small published value
// and acts on it.
//
// The invariant that proves the separation is not that a melody plays. It is
// that changing a slot's kind in the Scene changes nothing at all until
// somebody publishes it -- because the moment the audio path can be steered by
// state it does not own, every future feature can steer it by accident.

#include <cassert>
#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"
#include "src/state/material_slot_access.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialKind;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M3 FAIL: %s\n", message);
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

    // Silent patterns, so any onset heard on a voice came from its melody.
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

  int onsets(int voice, uint32_t fromTick, uint32_t toTick) {
    int count = 0;
    for (uint32_t tick = fromTick; tick <= toTick; ++tick) {
      queue.beginMidiRenderBlock(blockSequence++, 512,
                                 engine.transportPhaseSteps(), engine.bpm(),
                                 engine.sampleRate(), engine.isPlaying(),
                                 false, false);
      engine.currentTick_ = tick;
      engine.processSequencerEvents(tick);
      queue.endMidiRenderBlock();

      ScheduledMusicalEvent scheduled{};
      while (queue.tryPop(scheduled)) {
        const bool wanted =
            (voice == 0 && scheduled.event.target == MusicalEventTarget::SynthA) ||
            (voice == 1 && scheduled.event.target == MusicalEventTarget::SynthB);
        if (wanted && scheduled.event.type == MusicalEventType::NoteOn) ++count;
      }
      (void)queue.takePendingAllNotesOffMask();
    }
    return count;
  }

  void giveMelody(int voice, uint8_t note) {
    auto& melody = engine.currentPhraseBuffer(voice);
    melody = PhraseRuntime::RuntimeSynthEventBuffer{};
    melody.lengthTicks = PhraseRuntime::kTicksPerBar;
    melody.count = 1;
    melody.events[0].startTick = 0;
    melody.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
    melody.events[0].note = note;
    melody.events[0].velocity = 100;
    melody.events[0].probability = 100;
  }
};

}  // namespace

int main() {
  // 1. The authority starts on Pattern, per voice, and is readable.
  {
    Fixture fixture;
    for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
      expect(fixture.engine.activeMaterial(voice).kind == MaterialKind::Pattern,
             "a voice did not start on Pattern material");
    }
  }

  // 2. Publishing Melody is what makes the melody sound. Nothing else is
  //    consulted at play time.
  {
    Fixture fixture;
    fixture.giveMelody(0, 60);
    expect(fixture.onsets(0, 0, 40) == 0,
           "a melody sounded before its material was published");

    fixture.engine.publishActiveMaterial(0, 0, MaterialKind::Melody);
    expect(fixture.onsets(0, 384, 424) == 1,
           "publishing Melody did not make the melody sound");
  }

  // 3. Publishing back to Pattern silences it again, with the melody still in
  //    memory: the authority decides, not the presence of material.
  {
    Fixture fixture;
    fixture.giveMelody(0, 60);
    fixture.engine.publishActiveMaterial(0, 0, MaterialKind::Melody);
    (void)fixture.onsets(0, 0, 40);
    fixture.engine.publishActiveMaterial(0, 0, MaterialKind::Pattern);
    expect(fixture.onsets(0, 384, 424) == 0,
           "returning to Pattern still played the melody");
    expect(fixture.engine.currentPhraseBuffer(0).count == 1,
           "returning to Pattern destroyed the melody material");
  }

  // 4. The scene is not the authority. Marking a slot MELODY there must change
  //    nothing that sounds until it is published -- this is the separation the
  //    whole slice exists for.
  {
    Fixture fixture;
    fixture.giveMelody(0, 60);
    Scene& scene = fixture.engine.sceneManager_.currentScene();
    (void)GroovePuterMaterial::setResidentKind(scene, 0, 0,
                                               MaterialKind::Melody);
    expect(fixture.onsets(0, 0, 40) == 0,
           "a scene edit steered the audio path directly");
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
           "a scene edit changed the published authority behind its back");
  }

  // 5. Voices are independent: publishing one must not move the other.
  {
    Fixture fixture;
    fixture.giveMelody(0, 60);
    fixture.giveMelody(1, 67);
    fixture.engine.publishActiveMaterial(0, 0, MaterialKind::Melody);
    expect(fixture.engine.activeMaterial(1).kind == MaterialKind::Pattern,
           "publishing synth A moved synth B");
    expect(fixture.onsets(1, 384, 424) == 0,
           "synth B sounded a melody nobody published");
  }

  // 6. The published slot travels with the kind, so a later reader knows which
  //    material is live without asking anyone.
  {
    Fixture fixture;
    fixture.engine.publishActiveMaterial(1, 37, MaterialKind::Melody);
    expect(fixture.engine.activeMaterial(1).slot == 37,
           "the published slot was not retained");
  }

  if (g_failures == 0) {
    std::printf("M3 active material authority: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M3 active material authority: %d failure(s)\n",
               g_failures);
  return 1;
}
