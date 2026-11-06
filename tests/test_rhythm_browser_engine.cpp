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
  (void)queue.takePendingAllNotesOffMask();
  queue.beginMidiRenderBlock(1, 512, 0.0f, 120.0f, 44100.0f, true);
  engine.processSequencerEvents(0);
  queue.endMidiRenderBlock();
  assert(engine.auditionDrums_()->voices[0].steps[0].velocity == 37);
  assert(queue.takePendingAllNotesOffMask() == ScheduledMusicalEventQueue::kDrumsMask);
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
  assert(queue.takePendingAllNotesOffMask() == ScheduledMusicalEventQueue::kDrumsMask);
  assert(engine.endRhythmAudition(session));
  assert(queue.takePendingAllNotesOffMask() == ScheduledMusicalEventQueue::kDrumsMask);
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
  assert(engine.rhythmAuditionCleanupDue());
  engine.serviceRhythmAuditionCleanup();
  assert(!engine.rhythmAuditionCleanupDue());
  GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::Drums);
  engine.start();
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() == nullptr);
  assert(!engine.endRhythmAudition(stoppedSession));
  const uint32_t nextSession = engine.beginRhythmAudition();
  assert(nextSession != 0);
  assert(engine.endRhythmAudition(nextSession));
  engine.stop();

  // An accepted Phrase continues to play while preview drums are substituted.
  engine.setSongMode(false);
  engine.start();
  auto& phrase = engine.currentPhraseBuffer(0);
  phrase = PhraseRuntime::RuntimeSynthEventBuffer{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0].startTick = 0;
  phrase.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;
  engine.publishActiveMaterial(0, 0, GroovePuterMaterial::MaterialKind::Melody);
  const uint32_t phraseSession = engine.beginRhythmAudition();
  assert(phraseSession != 0);
  assert(engine.stageRhythmAudition(phraseSession, candidate));
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() != nullptr);
  assert(engine.patternPlaybackState_[0].active());
  assert(engine.patternPlaybackState_[0].activeNote() == 60);
  assert(engine.currentPhraseBuffer(0).count == 1);
  assert(engine.endRhythmAudition(phraseSession));
  engine.stop();

  // A Song row transition invalidates the old preview and releases its slots
  // through the guarded control service; it never redirects to another row.
  engine.setSongMode(false);
  engine.setSongLength(3);
  engine.setSongPattern(0, SongTrack::Drums, 0);
  engine.setSongPattern(1, SongTrack::Drums, 0);
  engine.setSongPattern(2, SongTrack::Drums, 1);
  engine.sceneManager().currentScene().feel.patternBars = 1;
  engine.setSongMode(true);
  engine.setSongPosition(0);
  engine.start();
  const uint32_t songSession = engine.beginRhythmAudition();
  assert(songSession != 0);
  assert(engine.stageRhythmAudition(songSession, candidate));
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() != nullptr);
  engine.processSequencerEvents(384);
  assert(engine.currentSongPosition() == 1);
  assert(engine.auditionDrums_() != nullptr);
  engine.processSequencerEvents(768);
  assert(engine.currentSongPosition() == 2);
  assert(engine.auditionDrums_() == nullptr);
  assert(engine.rhythmAuditionCleanupDue());
  engine.serviceRhythmAuditionCleanup();
  assert(!engine.rhythmAuditionCleanupDue());
  assert(!engine.endRhythmAudition(songSession));
  engine.stop();
  std::puts("Rhythm Browser engine audition: PASS");
}
