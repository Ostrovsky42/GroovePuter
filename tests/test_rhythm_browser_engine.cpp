#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"
#include "src/input/musical_event_queue.h"
#include "src/output/output_ownership.h"
#include "src/generation/rhythm_browser/rhythm_browser_catalog.h"
#include "src/ui/pages/drum_sequencer_page.h"

SerialMock Serial;
SDMock SD;

static float phase(void*) { return 0.0f; }

class BrowserTestGfx final : public IGfx {
 public:
  std::string text;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* value) override { if (value) text += value; }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int, int, int, int, IGfxColor) override {}
  void fillCircle(int, int, int, IGfxColor) override {}
  void drawLine(int32_t, int32_t, int32_t, int32_t, IGfxColor) override {}
  void setRotation(int) override {}
  void setTextColor(IGfxColor) override {}
  void setTextColor(uint16_t) override {}
  void setFont(GfxFont) override {}
  void startWrite() override {}
  void endWrite() override {}
  void flush() override {}
  int textWidth(const char* value) const override { return value ? int(std::strlen(value)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

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
  auto& mutableDrums = engine.sceneManager().currentScene().drumBanks[
      engine.sceneManager().getCurrentBankIndex(0)].patterns[
      engine.sceneManager().getCurrentDrumPatternIndex()];
  for (auto& lane : mutableDrums.lanes)
    lane.nodeCount = 0;
  auto& undo = GroovePuterUndo::undoOwner();
  undo.clear();
  const uint32_t receipt = 0x12345678u;
  assert(undo.commitRuntimePrepared(GroovePuterUndo::UndoKind::RuntimePhrase,
                                    receipt, [] {}));
  const auto revision = GroovePuterState::sceneRevisionSnapshot();
  const auto undoKind = undo.kind();
  const auto undoSize = undo.payloadSize();
  const auto undoRevision = undo.committedRevision();
  const bool undoRedo = undo.nextIsRedo();
  const Scene& scene = engine.sceneManager().currentScene();
  std::vector<unsigned char> beforeScene(sizeof(Scene));
  std::memcpy(beforeScene.data(), &scene, sizeof(Scene));
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
  assert(GroovePuterState::sceneRevisionSnapshot().persistedRevision == revision.persistedRevision);
  assert(std::memcmp(beforeScene.data(), &scene, sizeof(Scene)) == 0);
  assert(undo.hasUndo() && undo.kind() == undoKind && undo.payloadSize() == undoSize);
  assert(undo.committedRevision() == undoRevision && undo.nextIsRedo() == undoRedo);
  uint32_t retainedReceipt = 0;
  assert(undo.read(GroovePuterUndo::UndoKind::RuntimePhrase, retainedReceipt));
  assert(retainedReceipt == receipt);
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

  // A project reload or in-flight page change must not reuse the candidate.
  const uint32_t projectSession = engine.beginRhythmAudition();
  assert(projectSession != 0);
  assert(engine.stageRhythmAudition(projectSession, candidate));
  engine.processSequencerEvents(384);
  GroovePuterState::markSceneLoadSucceeded();
  engine.processSequencerEvents(408);
  assert(engine.auditionDrums_() == nullptr);
  assert(engine.rhythmAuditionCleanupDue());
  engine.serviceRhythmAuditionCleanup();
  const uint32_t pageSession = engine.beginRhythmAudition();
  assert(pageSession != 0);
  assert(engine.stageRhythmAudition(pageSession, candidate));
  engine.processSequencerEvents(768);
  engine.setPageLoading(true);
  engine.processSequencerEvents(792);
  assert(engine.auditionDrums_() == nullptr);
  assert(engine.rhythmAuditionCleanupDue());
  engine.serviceRhythmAuditionCleanup();
  engine.setPageLoading(false);
  engine.stop();

  // An accepted Phrase continues to play while preview drums are substituted.
  engine.setSongMode(false);
  GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::SynthA);
  assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA,
                                   GroovePuterOutput::Mode::Midi));
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
  queue.beginMidiRenderBlock(2000, 512, 0.0f, 120.0f, 44100.0f, true);
  engine.processSequencerEvents(0);
  queue.endMidiRenderBlock();
  bool heardPhraseMidi = false;
  while (queue.tryPop(event)) {
    if (event.event.target == MusicalEventTarget::SynthA &&
        event.event.type == MusicalEventType::NoteOn && event.event.note == 60)
      heardPhraseMidi = true;
  }
  assert(heardPhraseMidi);
  assert(engine.auditionDrums_() != nullptr);
  assert(engine.patternPlaybackState_[0].active());
  assert(engine.patternPlaybackState_[0].activeNote() == 60);
  assert(engine.currentPhraseBuffer(0).count == 1);
  assert(engine.setRhythmAuditionSolo(phraseSession, true));
  engine.liveNoteOn(1, 65, 100);
  queue.beginMidiRenderBlock(2001, 512, 0.0f, 120.0f, 44100.0f, true);
  engine.processSequencerEvents(384);
  queue.endMidiRenderBlock();
  assert(!engine.rhythmAuditionSolo(phraseSession));
  assert(engine.liveNote(1) == 65);
  bool phraseStillMidi = false;
  while (queue.tryPop(event)) {
    if (event.event.target == MusicalEventTarget::SynthA &&
        event.event.type == MusicalEventType::NoteOn && event.event.note == 60)
      phraseStillMidi = true;
  }
  assert(phraseStillMidi);
  engine.liveNoteOff(1, 65);
  queue.beginMidiRenderBlock(2002, 512, 0.0f, 120.0f, 44100.0f, true);
  engine.processSequencerEvents(768);
  queue.endMidiRenderBlock();
  assert(engine.rhythmAuditionSolo(phraseSession));
  assert(!engine.patternPlaybackState_[0].active());
  bool phraseNoteOff = false;
  while (queue.tryPop(event)) {
    assert(!(event.event.target == MusicalEventTarget::SynthA &&
             event.event.type == MusicalEventType::NoteOn));
    if (event.event.target == MusicalEventTarget::SynthA &&
        event.event.type == MusicalEventType::NoteOff && event.event.note == 60)
      phraseNoteOff = true;
  }
  assert(phraseNoteOff);
  engine.liveNoteOn(1, 67, 100);
  engine.processSequencerEvents(792);
  assert(engine.liveNote(1) == 67);
  engine.liveNoteOff(1, 67);
  assert(engine.setRhythmAuditionSolo(phraseSession, false));
  engine.processSequencerEvents(1152);
  assert(!engine.rhythmAuditionSolo(phraseSession));
  assert(engine.patternPlaybackState_[0].active());
  assert(engine.endRhythmAudition(phraseSession));
  engine.stop();
  GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::SynthA);

  // Preview follows Song while the original destination remains pinned; the
  // current row's automation applies, and a missing drum target ends preview.
  engine.setSongMode(false);
  engine.setSongLength(4);
  engine.setSongPattern(0, SongTrack::Drums, 0);
  engine.setSongPattern(1, SongTrack::Drums, 0);
  engine.setSongPattern(2, SongTrack::Drums, 1);
  engine.clearSongPattern(3, SongTrack::Drums);
  auto& lane = engine.sceneManager().currentScene().drumBanks[0].patterns[1].lanes[0];
  lane.targetParam = DRUM_AUTOMATION_REVERB_MIX;
  lane.nodeCount = 1;
  lane.nodes[0].step = 0;
  lane.nodes[0].value = 0.72f;
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
  assert(engine.auditionDrums_() != nullptr);
  assert(engine.sceneManager().currentScene().drumFX.reverbMix == 0.72f);
  engine.processSequencerEvents(1152);
  assert(engine.currentSongPosition() == 3);
  assert(engine.auditionDrums_() == nullptr);
  assert(engine.rhythmAuditionCleanupDue());
  engine.serviceRhythmAuditionCleanup();
  assert(!engine.rhythmAuditionCleanupDue());
  assert(!engine.endRhythmAudition(songSession));
  engine.stop();

  // Exercise the actual named catalog material through the engine and MIDI
  // producer, with an empty accepted drum pattern as the control.
  engine.setSongMode(false);
  auto& catalogDrums = engine.sceneManager().currentScene().drumBanks[
      engine.sceneManager().getCurrentBankIndex(0)].patterns[
      engine.sceneManager().getCurrentDrumPatternIndex()];
  catalogDrums = DrumPatternSet{};
  RhythmBrowser::Descriptor descriptor{};
  assert(RhythmBrowser::lookup(RhythmBrowser::Source::Vocabulary, 401, descriptor));
  assert(descriptor.name && descriptor.name[0]);
  RhythmBrowser::Workspace workspace{};
  RhythmBrowser::Candidate catalogCandidate{};
  const RhythmBrowser::Identity identity{
      descriptor.source, descriptor.stableId, 0, 42,
      GroovePuterState::sceneRevisionSnapshot().currentRevision};
  assert(RhythmBrowser::materialize(identity, workspace, catalogCandidate) ==
         RhythmBrowser::Status::Ok);
  assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::Drums,
                                   GroovePuterOutput::Mode::Midi));
  engine.start();
  std::vector<unsigned char> catalogScene(sizeof(Scene));
  std::memcpy(catalogScene.data(), &engine.sceneManager().currentScene(), sizeof(Scene));
  const uint32_t catalogSession = engine.beginRhythmAudition();
  assert(catalogSession != 0);
  assert(engine.stageRhythmAudition(catalogSession, catalogCandidate.drums));
  int observedHits = 0;
  for (uint32_t tick = 0; tick < 384; ++tick) {
    queue.beginMidiRenderBlock(100 + tick, 512, 0.0f, 120.0f, 44100.0f, true);
    engine.processSequencerEvents(tick);
    queue.endMidiRenderBlock();
    ScheduledMusicalEvent emitted{};
    while (queue.tryPop(emitted)) {
      if (emitted.event.type == MusicalEventType::NoteOn &&
          emitted.event.target == MusicalEventTarget::Drums) ++observedHits;
    }
  }
  assert(observedHits > 0);
  assert(engine.endRhythmAudition(catalogSession));
  assert(std::memcmp(catalogScene.data(), &engine.sceneManager().currentScene(),
                     sizeof(Scene)) == 0);
  engine.stop();
  GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::Drums);

  BrowserTestGfx gfx;
  DrumSequencerPage drumPage(gfx, engine, AudioGuard{});
  drumPage.setBoundaries(Rect{0, 0, 240, 135});
  drumPage.onEnter(0);
  std::vector<unsigned char> browserScene(sizeof(Scene));
  std::memcpy(browserScene.data(), &engine.sceneManager().currentScene(), sizeof(Scene));
  UIEvent open{};
  open.event_type = GROOVEPUTER_KEY_DOWN;
  open.key = 'r';
  open.alt = true;
  assert(drumPage.handleEvent(open));
  drumPage.draw(gfx);
  assert(gfx.text.find("RHYTHM") != std::string::npos);
  UIEvent listen{};
  listen.event_type = GROOVEPUTER_KEY_DOWN;
  listen.key = '\t';
  listen.scancode = GROOVEPUTER_TAB;
  assert(drumPage.handleEvent(listen));
  assert(engine.isPlaying());
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() != nullptr);
  const DrumPatternSet firstBrowserPattern = *engine.auditionDrums_();
  UIEvent down{};
  down.event_type = GROOVEPUTER_KEY_DOWN;
  down.scancode = GROOVEPUTER_DOWN;
  assert(drumPage.handleEvent(down));
  engine.processSequencerEvents(24);
  assert(std::memcmp(firstBrowserPattern.voices, engine.auditionDrums_()->voices,
                     sizeof(firstBrowserPattern.voices)) == 0);
  engine.processSequencerEvents(384);
  bool different = false;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice)
    for (int step = 0; step < DrumPattern::kSteps; ++step)
      different |= firstBrowserPattern.voices[voice].steps[step].hit !=
                   engine.auditionDrums_()->voices[voice].steps[step].hit;
  assert(different);
  UIEvent cancel{};
  cancel.event_type = GROOVEPUTER_KEY_DOWN;
  cancel.key = 0x1b;
  assert(drumPage.handleEvent(cancel));
  assert(!engine.isPlaying());
  assert(engine.auditionDrums_() == nullptr);
  assert(std::memcmp(browserScene.data(), &engine.sceneManager().currentScene(),
                     sizeof(Scene)) == 0);
  engine.start();
  assert(drumPage.handleEvent(open));
  assert(drumPage.handleEvent(listen));
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() != nullptr);
  drumPage.onExit();
  assert(engine.isPlaying());
  assert(engine.auditionDrums_() == nullptr);
  engine.stop();
  std::puts("Rhythm Browser engine audition: PASS");
}
