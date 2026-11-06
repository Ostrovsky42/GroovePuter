#include <cassert>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/state/scene_revision.h"
#include "src/input/musical_event_queue.h"
#include "src/output/output_ownership.h"

SerialMock Serial;
SDMock SD;

static float phase(void*) { return 0.0f; }

int main() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  engine.setSongMode(false);
  MusicalEventQueue queue;
  queue.setPhaseReader(phase, nullptr);
  engine.setPatternEventQueue(&queue);
  GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::Drums);
  assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::Drums,
                                   GroovePuterOutput::Mode::Midi));
  engine.start();
  const auto revision = GroovePuterState::sceneRevisionSnapshot();
  const DrumPatternSet accepted = engine.sceneManager().getCurrentDrumPattern();
  const int noteA = engine.activeSynthPattern(0).steps[0].note;
  const int noteB = engine.activeSynthPattern(1).steps[0].note;
  const uint32_t session = engine.beginRhythmAudition();
  assert(session != 0);
  DrumPatternSet candidate{};
  candidate.voices[0].steps[0].hit = true;
  candidate.voices[0].steps[0].velocity = 37;
  assert(engine.stageRhythmAudition(session, candidate));
  assert(engine.activeDrumPattern(0).steps[0].velocity == accepted.voices[0].steps[0].velocity);
  queue.beginMidiRenderBlock(1, 512, 0.0f, 120.0f, 44100.0f, true);
  engine.processSequencerEvents(0);
  queue.endMidiRenderBlock();
  assert(engine.auditionDrums_()->voices[0].steps[0].velocity == 37);
  bool sawPreviewKick = false;
  ScheduledMusicalEvent event{};
  while (queue.tryPop(event)) {
    if (event.event.target == MusicalEventTarget::Drums &&
        event.event.channel == 0 && event.event.velocity == 37) sawPreviewKick = true;
  }
  assert(sawPreviewKick);
  candidate.voices[0].steps[0].velocity = 79;
  assert(engine.stageRhythmAudition(session, candidate));
  engine.processSequencerEvents(24);
  assert(engine.auditionDrums_()->voices[0].steps[0].velocity == 37);
  engine.processSequencerEvents(384);
  assert(engine.auditionDrums_()->voices[0].steps[0].velocity == 79);
  assert(engine.endRhythmAudition(session));
  assert(engine.activeDrumPattern(0).steps[0].velocity == accepted.voices[0].steps[0].velocity);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == revision.currentRevision);
  assert(engine.sceneManager().getCurrentDrumPattern().voices[0].steps[0].velocity == accepted.voices[0].steps[0].velocity);
  assert(engine.activeSynthPattern(0).steps[0].note == noteA);
  assert(engine.activeSynthPattern(1).steps[0].note == noteB);
  const uint32_t stoppedSession = engine.beginRhythmAudition();
  assert(stoppedSession != 0);
  assert(engine.stageRhythmAudition(stoppedSession, candidate));
  engine.processSequencerEvents(768);
  assert(engine.auditionDrums_() != nullptr);
  engine.stop();
  GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::Drums);
  engine.start();
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() == nullptr);
  assert(engine.endRhythmAudition(stoppedSession));
  engine.stop();
  std::puts("Rhythm Browser engine audition: PASS");
}
