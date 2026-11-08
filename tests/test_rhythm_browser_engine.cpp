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
#define private public
#include "src/ui/pages/drum_sequencer_page.h"
#undef private

SerialMock Serial;
SDMock SD;

static float phase(void*) { return 0.0f; }

class BrowserTestGfx final : public IGfx {
 public:
  std::string text;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  int maxRight = 0;
  // Right edge of text in the browser's middle band (y 27..67: names, info,
  // hear line), which must stay clear of the miniature at x 186.
  int bandRight = 0;
  // Miniature cells: fills at x >= 180 (solid hit 2x3, soft hit 2x1).
  int miniHits = 0;
  int miniSoft = 0;
  void drawText(int x, int y, const char* value) override {
    if (!value) return;
    text += value;
    const int right = x + int(std::strlen(value)) * 6;
    if (right > maxRight) maxRight = right;
    if (y >= 27 && y <= 67 && right > bandRight) bandRight = right;
  }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int x, int, int, int h, IGfxColor) override {
    if (x < 180) return;
    if (h == 1) ++miniSoft;
    else ++miniHits;
  }
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
  // Bare Opt (TOGGLE_SOURCE from firmware) opens the browser on the grid too.
  UIEvent optTap{};
  optTap.event_type = GROOVEPUTER_APPLICATION_EVENT;
  optTap.app_event_type = GROOVEPUTER_APP_EVENT_TOGGLE_SOURCE;
  assert(drumPage.handleEvent(optTap));
  gfx.text.clear();
  drumPage.draw(gfx);
  assert(gfx.text.find("RHYTHMS") != std::string::npos);
  UIEvent closeOpt{};
  closeOpt.event_type = GROOVEPUTER_KEY_DOWN;
  closeOpt.scancode = GROOVEPUTER_ESCAPE;
  assert(drumPage.handleEvent(closeOpt));
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
  gfx.text.clear();
  drumPage.draw(gfx);
  assert(gfx.text.find("PENDING NEXT BAR") != std::string::npos);
  UIEvent globalTab = listen;
  globalTab.meta = true;
  assert(!drumPage.handleEvent(globalTab));
  engine.processSequencerEvents(0);
  assert(engine.auditionDrums_() != nullptr);
  gfx.text.clear();
  drumPage.draw(gfx);
  assert(gfx.text.find("WITH SONG") != std::string::npos);
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

  // Browser navigation: Opt closes, the position is remembered between
  // openings, G keeps the current rhythm when it belongs to the new group, and
  // an empty group shows EMPTY instead of a foreign rhythm.
  {
    drumPage.onEnter(0);
    drumPage.browserIndex_ = drumPage.browserVariant_ = drumPage.browserGroup_ = 0;
    assert(drumPage.handleEvent(open));
    assert(drumPage.browserOpen_);
    assert(drumPage.handleEvent(optTap));
    assert(!drumPage.browserOpen_);  // Opt closes, like Esc
    assert(drumPage.handleEvent(open));
    assert(drumPage.handleEvent(down));
    assert(drumPage.handleEvent(down));
    const uint8_t remembered = drumPage.browserIndex_;
    assert(remembered != 0);
    assert(drumPage.handleEvent(closeOpt));
    assert(drumPage.handleEvent(open));
    assert(drumPage.browserIndex_ == remembered);  // position survives close

    // G: stay on the current rhythm when it is in the next group.
    int inHouse = -1;
    for (int i = 0; i < RhythmBrowser::count() && inHouse < 0; ++i) {
      RhythmBrowser::Descriptor d{};
      if (RhythmBrowser::descriptorAt(static_cast<uint8_t>(i), d) &&
          RhythmBrowser::matchesGroup(d, RhythmBrowser::Group::House)) inHouse = i;
    }
    assert(inHouse >= 0);
    drumPage.browserGroup_ = 0;
    drumPage.browserIndex_ = static_cast<uint8_t>(inHouse);
    UIEvent gKey{};
    gKey.event_type = GROOVEPUTER_KEY_DOWN;
    gKey.key = 'g';
    assert(drumPage.handleEvent(gKey));
    assert(drumPage.browserGroup_ == 1);
    assert(drumPage.browserIndex_ == inHouse);

    // Alt+G walks the groups backwards, wrapping below ALL; other Alt chords
    // stay unhandled.
    UIEvent altG = gKey;
    altG.alt = true;
    assert(drumPage.handleEvent(altG));
    assert(drumPage.browserGroup_ == 0);
    assert(drumPage.handleEvent(altG));
    assert(drumPage.browserGroup_ ==
           static_cast<uint8_t>(RhythmBrowser::Group::Count) - 1);
    assert(drumPage.handleEvent(gKey));  // forward wraps to ALL again
    assert(drumPage.browserGroup_ == 0);
    UIEvent altX = gKey;
    altX.alt = true;
    altX.key = 'x';
    assert(!drumPage.handleEvent(altX));
    // The info line carries no ATLAS / PRESET / VOCAB source label.
    drumPage.browserIndex_ = static_cast<uint8_t>(inHouse);
    gfx.text.clear();
    drumPage.draw(gfx);
    assert(gfx.text.find("VAR ") != std::string::npos);
    assert(gfx.text.find("ATLAS") == std::string::npos);
    assert(gfx.text.find("PRESET") == std::string::npos);
    assert(gfx.text.find("VOCAB") == std::string::npos);
    assert(gfx.text.find("G/Alt+G GRP") != std::string::npos);
    drumPage.browserGroup_ = 1;
    drumPage.browserIndex_ = static_cast<uint8_t>(inHouse);

    // An empty group (if the catalog has one) shows EMPTY and cannot APPLY.
    for (uint8_t g = 1; g < static_cast<uint8_t>(RhythmBrowser::Group::Count); ++g) {
      bool any = false;
      for (int i = 0; i < RhythmBrowser::count(); ++i) {
        RhythmBrowser::Descriptor d{};
        any |= RhythmBrowser::descriptorAt(static_cast<uint8_t>(i), d) &&
               RhythmBrowser::matchesGroup(d, static_cast<RhythmBrowser::Group>(g));
      }
      if (any) continue;
      drumPage.browserGroup_ = static_cast<uint8_t>(g - 1);
      assert(drumPage.handleEvent(gKey));
      assert(drumPage.browserGroup_ == g);
      gfx.text.clear();
      drumPage.draw(gfx);
      assert(gfx.text.find("EMPTY") != std::string::npos);
      assert(!drumPage.browserSelectionInGroup_());
      break;
    }
    drumPage.browserGroup_ = 0;
    drumPage.browserIndex_ = drumPage.browserVariant_ = 0;
    assert(drumPage.handleEvent(closeOpt));
    drumPage.onExit();
    std::puts("Rhythm Browser Opt/position/G/EMPTY: PASS");
  }

  // APPLY (Enter): the selected rhythm replaces the 8 voices of the current
  // drum pattern, keeps lanes/groove, is one Pattern Undo step, and closes the
  // browser. The expected voices use the browser's fixed hearing seed.
  {
    drumPage.onEnter(0);
    SceneManager& manager = engine.sceneManager();
    const int bank = manager.getCurrentBankIndex(0);
    const int slot = manager.getCurrentDrumPatternIndex();
    DrumPatternSet& live = manager.currentScene().drumBanks[bank].patterns[slot];
    live.groove.swing = 0.25f;
    live.lanes[0].targetParam = DRUM_AUTOMATION_REVERB_MIX;
    live.lanes[0].nodeCount = 1;
    const DrumPatternSet beforeApply = live;

    RhythmBrowser::Descriptor first{};
    assert(RhythmBrowser::descriptorAt(0, first));
    RhythmBrowser::Workspace applyWorkspace{};
    RhythmBrowser::Candidate expected{};
    assert(RhythmBrowser::materialize(
               {first.source, first.stableId, 0, 0x52485954u, 0},
               applyWorkspace, expected) == RhythmBrowser::Status::Ok);
    assert(std::memcmp(beforeApply.voices, expected.drums.voices,
                       sizeof(beforeApply.voices)) != 0);

    assert(drumPage.handleEvent(open));
    gfx.text.clear();
    gfx.maxRight = 0;
    drumPage.draw(gfx);
    char target[32];
    std::snprintf(target, sizeof(target), "ENTER: ALL 8 VOICES -> DRUMS %c%d",
                  bank == 1 ? 'B' : 'A', slot + 1);
    assert(gfx.text.find(target) != std::string::npos);
    assert(gfx.text.find("UNAVAILABLE") == std::string::npos);
    assert(gfx.text.find("VAR 1/") != std::string::npos);
    assert(gfx.maxRight <= 240);
    // Every catalog entry's browser page fits the 240 px screen (5x7 font,
    // 6 px advance). Walking a full cycle returns to entry 0.
    for (int n = 0; n < RhythmBrowser::count(); ++n) {
      assert(drumPage.handleEvent(down));
      gfx.text.clear();
      gfx.maxRight = 0;
      gfx.bandRight = 0;
      drumPage.draw(gfx);
      assert(gfx.maxRight <= 240);
      assert(gfx.bandRight < 186);  // text never runs into the miniature
    }

    UIEvent enter{};
    enter.event_type = GROOVEPUTER_KEY_DOWN;
    enter.key = '\n';
    assert(drumPage.handleEvent(enter));
    assert(std::memcmp(live.voices, expected.drums.voices, sizeof(live.voices)) == 0);
    assert(live.groove.swing == 0.25f);
    assert(live.lanes[0].targetParam == DRUM_AUTOMATION_REVERB_MIX);
    assert(live.lanes[0].nodeCount == 1);
    assert(GroovePuterUndo::undoOwner().hasUndo());
    assert(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Pattern);
    gfx.text.clear();
    drumPage.draw(gfx);
    assert(gfx.text.find("ENTER APPLY") == std::string::npos);  // back on the grid

    UIEvent undo{};
    undo.event_type = GROOVEPUTER_APPLICATION_EVENT;
    undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    assert(drumPage.handleEvent(undo));
    assert(std::memcmp(&live, &beforeApply, sizeof(DrumPatternSet)) == 0);
    assert(drumPage.handleEvent(undo));  // redo
    assert(std::memcmp(live.voices, expected.drums.voices, sizeof(live.voices)) == 0);
    assert(drumPage.handleEvent(undo));
    assert(std::memcmp(&live, &beforeApply, sizeof(DrumPatternSet)) == 0);

    // APPLY while hearing: the audition overlay ends, transport keeps running
    // and the real drum track now carries the rhythm.
    engine.start();
    assert(drumPage.handleEvent(open));
    assert(drumPage.handleEvent(listen));
    engine.processSequencerEvents(0);
    assert(engine.auditionDrums_() != nullptr);
    assert(drumPage.handleEvent(enter));
    assert(engine.isPlaying());
    assert(engine.auditionDrums_() == nullptr);
    assert(std::memcmp(live.voices, expected.drums.voices, sizeof(live.voices)) == 0);
    gfx.text.clear();
    drumPage.draw(gfx);
    assert(gfx.text.find("ENTER APPLY") == std::string::npos);
    assert(drumPage.handleEvent(undo));
    assert(std::memcmp(&live, &beforeApply, sizeof(DrumPatternSet)) == 0);
    engine.stop();
    drumPage.onExit();
    std::puts("Rhythm Browser APPLY + undo: PASS");
  }

  // Audition mutes: 1/2 song synths, 3..0 the candidate's drum voices. They
  // reuse the engine track mutes, end with the audition and restore the state
  // from before HEAR; APPLY still writes all 8 voices.
  {
    drumPage.onEnter(0);
    SceneManager& manager = engine.sceneManager();
    DrumPatternSet& live = manager.currentScene().drumBanks[manager.getCurrentBankIndex(0)]
        .patterns[manager.getCurrentDrumPatternIndex()];
    RhythmBrowser::Descriptor first{};
    assert(RhythmBrowser::descriptorAt(0, first));
    RhythmBrowser::Workspace muteWorkspace{};
    RhythmBrowser::Candidate expected{};
    assert(RhythmBrowser::materialize(
               {first.source, first.stableId, 0, 0x52485954u, 0},
               muteWorkspace, expected) == RhythmBrowser::Status::Ok);
    int kickHits = 0;
    int otherHits = 0;
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      kickHits += expected.drums.voices[0].steps[step].hit;
      for (int voice = 1; voice < DrumPatternSet::kVoices; ++voice)
        otherHits += voice != 1 && expected.drums.voices[voice].steps[step].hit;
    }
    assert(kickHits > 0 && otherHits > 0);

    for (int i = 0; i < 10; ++i) assert(engine.isTrackActive(i));
    engine.setTrackMuted(3, true);  // snare muted before the browser opens
    assert(engine.isSnareMuted());
    std::vector<unsigned char> muteScene(sizeof(Scene));
    std::memcpy(muteScene.data(), &manager.currentScene(), sizeof(Scene));

    const auto key = [](char value) {
      UIEvent e{};
      e.event_type = GROOVEPUTER_KEY_DOWN;
      e.key = value;
      return e;
    };
    const auto countDrumHits = [&](uint32_t startTick, int counts[8]) {
      for (int v = 0; v < 8; ++v) counts[v] = 0;
      for (uint32_t tick = startTick; tick < startTick + 384; ++tick) {
        queue.beginMidiRenderBlock(5000 + tick, 512, 0.0f, 120.0f, 44100.0f, true);
        engine.processSequencerEvents(tick);
        queue.endMidiRenderBlock();
        ScheduledMusicalEvent emitted{};
        while (queue.tryPop(emitted)) {
          if (emitted.event.type == MusicalEventType::NoteOn &&
              emitted.event.target == MusicalEventTarget::Drums &&
              emitted.event.channel < 8) ++counts[emitted.event.channel];
        }
      }
    };

    assert(drumPage.handleEvent(open));
    UIEvent kickKey = key('3');
    assert(drumPage.handleEvent(kickKey));  // not hearing yet: no change
    assert(!engine.isKickMuted());
    gfx.text.clear();
    drumPage.draw(gfx);
    // The browser draws no mute rows or hints; mute state lives on the track
    // screens and the 1-0 hint is in the status line only.
    assert(gfx.text.find("1-0 MUTE") == std::string::npos);
    assert(gfx.text.find("TAB HEAR  M SOLO") == std::string::npos);

    // Miniature of the first entry (variant 0) matches the candidate that
    // hearing and APPLY materialize: one fill per hit, soft hits as 2x1 dots.
    {
      RhythmBrowser::Descriptor shown{};
      assert(RhythmBrowser::descriptorAt(0, shown));
      RhythmBrowser::Workspace miniWorkspace{};
      RhythmBrowser::Candidate miniExpected{};
      assert(RhythmBrowser::materialize(
                 {shown.source, shown.stableId, 0, 0x52485954u,
                  GroovePuterState::sceneRevisionSnapshot().currentRevision},
                 miniWorkspace, miniExpected) == RhythmBrowser::Status::Ok);
      int loud = 0;
      int soft = 0;
      for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice)
        for (int step = 0; step < DrumPattern::kSteps; ++step) {
          const DrumStep& cell = miniExpected.drums.voices[voice].steps[step];
          if (!cell.hit) continue;
          if (cell.velocity < 64) ++soft;
          else ++loud;
        }
      assert(loud + soft > 0);
      gfx.miniHits = gfx.miniSoft = 0;
      drumPage.draw(gfx);
      assert(gfx.miniHits == loud);
      assert(gfx.miniSoft == soft);
    }

    assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::Drums,
                                     GroovePuterOutput::Mode::Midi));
    engine.start();
    assert(drumPage.handleEvent(listen));
    int counts[8];
    countDrumHits(0, counts);  // candidate becomes audible at the bar
    assert(engine.auditionDrums_() != nullptr);
    countDrumHits(384, counts);
    assert(counts[0] > 0);
    assert(counts[1] == 0);  // snare stays muted as before HEAR

    assert(drumPage.handleEvent(kickKey));
    assert(engine.isKickMuted());
    UIEvent snareKey = key('4');
    assert(drumPage.handleEvent(snareKey));
    assert(!engine.isSnareMuted());
    countDrumHits(768, counts);
    assert(counts[0] == 0);  // muted rhythm voice is silent
    int otherHeard = 0;
    for (int v = 2; v < 8; ++v) otherHeard += counts[v];
    assert(otherHeard > 0);  // unmuted voices still sound

    // Song tracks: 1 mutes SYN-A, whose onsets then stop reaching the output.
    UIEvent synthKey = key('1');
    assert(drumPage.handleEvent(synthKey));
    assert(engine.is303Muted(0));
    assert(!engine.is303Muted(1));
    assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA,
                                     GroovePuterOutput::Mode::Midi));
    PhraseRuntime::RuntimeSynthEvent synthEvent{};
    synthEvent.durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
    synthEvent.note = 62;
    synthEvent.velocity = 100;
    synthEvent.probability = 100;
    const auto synthHeard = [&]() {
      queue.beginMidiRenderBlock(9000, 512, 0.0f, 120.0f, 44100.0f, true);
      engine.triggerSynthStep_(0, synthEvent, 0);
      queue.endMidiRenderBlock();
      bool heard = false;
      ScheduledMusicalEvent emitted{};
      while (queue.tryPop(emitted))
        heard |= emitted.event.target == MusicalEventTarget::SynthA &&
                 emitted.event.type == MusicalEventType::NoteOn && emitted.event.note == 62;
      return heard;
    };
    assert(!synthHeard());

    // While hearing the browser shows no mute tokens (no duplicate of the
    // track-screen overlay), fits the 240 px screen, and keeps the miniature.
    gfx.text.clear();
    gfx.maxRight = 0;
    gfx.bandRight = 0;
    gfx.miniHits = gfx.miniSoft = 0;
    drumPage.draw(gfx);
    assert(gfx.text.find("1SYN-A") == std::string::npos);
    assert(gfx.text.find("2SYN-B") == std::string::npos);
    assert(gfx.text.find("3KIK") == std::string::npos);
    assert(gfx.text.find("0CLP") == std::string::npos);
    assert(gfx.maxRight <= 240);
    assert(gfx.bandRight < 186);
    assert(gfx.miniHits + gfx.miniSoft > 0);

    // Esc ends the audition and restores the mutes from before HEAR.
    UIEvent esc{};
    esc.event_type = GROOVEPUTER_KEY_DOWN;
    esc.scancode = GROOVEPUTER_ESCAPE;
    assert(drumPage.handleEvent(esc));
    assert(!engine.isKickMuted());
    assert(engine.isSnareMuted());
    assert(!engine.is303Muted(0));
    assert(std::memcmp(muteScene.data(), &manager.currentScene(), sizeof(Scene)) == 0);
    engine.start();
    engine.processSequencerEvents(0);
    (void)queue.takePendingAllNotesOffMask();
    while (true) { ScheduledMusicalEvent drop{}; if (!queue.tryPop(drop)) break; }
    assert(synthHeard());

    // Tab again (stop hearing) restores too.
    assert(drumPage.handleEvent(open));
    assert(drumPage.handleEvent(listen));
    assert(drumPage.handleEvent(kickKey));
    assert(engine.isKickMuted());
    assert(drumPage.handleEvent(listen));
    assert(!engine.isKickMuted());

    // Transport stopping under the audition restores on the next tick.
    engine.start();
    assert(drumPage.handleEvent(listen));
    engine.processSequencerEvents(0);
    assert(drumPage.handleEvent(kickKey));
    assert(engine.isKickMuted());
    engine.stop();
    drumPage.tick();
    assert(!engine.isKickMuted());
    assert(engine.isSnareMuted());
    assert(drumPage.handleEvent(esc));

    // APPLY with voices muted writes the full candidate and restores mutes.
    engine.start();
    assert(drumPage.handleEvent(open));
    assert(drumPage.handleEvent(listen));
    engine.processSequencerEvents(0);
    assert(drumPage.handleEvent(kickKey));
    UIEvent hatKey = key('5');
    assert(drumPage.handleEvent(hatKey));
    assert(engine.isKickMuted() && engine.isHatMuted());
    UIEvent enter{};
    enter.event_type = GROOVEPUTER_KEY_DOWN;
    enter.key = '\n';
    assert(drumPage.handleEvent(enter));
    assert(std::memcmp(live.voices, expected.drums.voices, sizeof(live.voices)) == 0);
    assert(!engine.isKickMuted() && !engine.isHatMuted());
    assert(engine.isSnareMuted());
    UIEvent undo{};
    undo.event_type = GROOVEPUTER_APPLICATION_EVENT;
    undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    assert(drumPage.handleEvent(undo));
    engine.stop();
    drumPage.onExit();
    engine.setTrackMuted(3, false);
    GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::Drums);
    GroovePuterOutput::restoreLegacyCompatibility(GroovePuterOutput::Track::SynthA);
    std::puts("Rhythm Browser audition mutes: PASS");
  }
}
