// STEPS PAIR PLAY (plain L, with the pair view): one voice loops slot N, N+1 on the bar line. It is
// a control-side override: the selected slot, the other voice and the drums
// never move, and Song / Melody / unsaved edits refuse it.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/state/material_slot_access.h"
#include "src/ui/pages/pattern_edit_page.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {

class NullGfx final : public IGfx {
 public:
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char*) override {}
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
  int textWidth(const char* v) const override { return v ? (int)std::strlen(v) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

bool g_ok = true;
void expect(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    g_ok = false;
  }
}

void keyL(SynthSequencerPage& page, bool ctrl) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = 'l';
  e.scancode = GROOVEPUTER_L;
  e.ctrl = ctrl;
  (void)page.handleEvent(e);
}
void plainL(SynthSequencerPage& page) { keyL(page, false); }
void ctrlL(SynthSequencerPage& page) { keyL(page, true); }

int patA(MiniAcid& e) { return e.songPatternIndexForTrack(SongTrack::SynthA); }
int patB(MiniAcid& e) { return e.songPatternIndexForTrack(SongTrack::SynthB); }
int patD(MiniAcid& e) { return e.songPatternIndexForTrack(SongTrack::Drums); }

}  // namespace

int main() {
  // 1. Four bars: 2, 3, 2, 3 on voice A; B, drums and the selector stay.
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, 2);
    engine.set303PatternIndex(1, 5);
    const int drums = patD(engine);
    NullGfx gfx;
    SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
    page.setBoundaries(Rect{0, 16, 240, 93});
    page.onEnter(0);

    plainL(page);
    expect(engine.pairPlayActive(0), "L starts pair play");
    expect(!engine.pairPlayActive(1), "voice B is not in pair play");
    engine.start();
    int seenA[4], seenB[4];
    for (int bar = 0; bar < 4; ++bar) {
      engine.processSequencerEvents(static_cast<uint32_t>(bar) * 384u);
      seenA[bar] = patA(engine);
      seenB[bar] = patB(engine);
      expect(patD(engine) == drums, "drum index unchanged");
      expect(engine.current303PatternIndex(0) == 2, "selector stays on slot 2");
      expect(engine.current303PatternIndex(1) == 5, "voice B selector unchanged");
    }
    expect(seenA[0] == 2 && seenA[1] == 3 && seenA[2] == 2 && seenA[3] == 3,
           "voice A plays 2, 3, 2, 3");
    for (int bar = 0; bar < 4; ++bar) expect(seenB[bar] == 5, "voice B stays on 5");

    plainL(page);
    expect(!engine.pairPlayActive(0), "L again stops the pair");
    expect(patA(engine) == 2, "stopped pair returns to the selected slot");

    // Cancel on Stop.
    plainL(page);
    expect(engine.pairPlayActive(0), "restart");
    engine.stop();
    expect(!engine.pairPlayActive(0), "transport Stop cancels");

    // Cancel on slot change and on leaving the page.
    plainL(page);
    engine.set303PatternIndex(0, 4);
    expect(!engine.pairPlayActive(0), "changing slot cancels");
    engine.set303PatternIndex(0, 2);
    plainL(page);
    expect(engine.pairPlayActive(0), "restart before leaving");
    page.onExit();
    expect(!engine.pairPlayActive(0), "leaving the page cancels");

    // Song on cancels and refuses.
    plainL(page);
    engine.setSongMode(true);
    expect(!engine.pairPlayActive(0), "Song on cancels");
    plainL(page);
    expect(!engine.pairPlayActive(0), "Song on refuses");
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::SongOn,
           "Song on reports SongOn");
    engine.setSongMode(false);
  }

  // 1c. Plain L is view + play in one key; Ctrl+L does nothing.
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, 2);
    NullGfx gfx;
    PatternEditPage steps(gfx, engine, AudioGuard{}, 0);
    const auto key = [&](bool ctrl) {
      UIEvent e{};
      e.event_type = GROOVEPUTER_KEY_DOWN;
      e.key = 'l';
      e.scancode = GROOVEPUTER_L;
      e.ctrl = ctrl;
      (void)steps.handleEvent(e);
    };
    key(true);
    expect(!engine.pairPlayActive(0) && !steps.pairView(), "Ctrl+L does nothing when off");
    key(false);
    expect(engine.pairPlayActive(0) && steps.pairView(), "L: view on and play on");
    key(true);
    expect(engine.pairPlayActive(0) && steps.pairView(), "Ctrl+L does not stop");
    key(false);
    expect(!engine.pairPlayActive(0) && !steps.pairView(), "L again: play off and view off");
    // Refusal keeps the view on.
    engine.setSongMode(true);
    key(false);
    expect(!engine.pairPlayActive(0) && steps.pairView(), "refused play keeps the view on");
    key(false);
    expect(!steps.pairView(), "L turns the kept view off");
    engine.setSongMode(false);
  }

  // 1b. The playhead follows the sounding slot: rows 3-4 in phase 1.
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, 2);
    NullGfx gfx;
    PatternEditPage steps(gfx, engine, AudioGuard{}, 0);
    expect(steps.playheadPair() == 0, "pair view off: normal playhead");
    steps.togglePairView();
    expect(steps.playheadPair() == 0, "pair play off: normal playhead");
    expect(engine.pairPlayPhase(0) == -1, "phase is -1 when pair play is off");
    SynthSequencerPage host(gfx, engine, AudioGuard{}, 0);
    host.setBoundaries(Rect{0, 16, 240, 93});
    host.onEnter(0);
    plainL(host);
    expect(engine.pairPlayActive(0), "pair play starts");
    engine.start();
    engine.processSequencerEvents(0u);
    expect(engine.pairPlayPhase(0) == 0 && steps.playheadPair() == 0,
           "phase 0: playhead in rows 1-2");
    engine.processSequencerEvents(384u);
    expect(engine.pairPlayPhase(0) == 1 && steps.playheadPair() == 1,
           "phase 1: playhead in rows 3-4");
    engine.processSequencerEvents(384u + 5u * 24u);
    expect(engine.currentStep() >= 0 && engine.currentStep() < 16 &&
               steps.playheadPair() == 1,
           "phase 1 holds across the bar, step index unchanged");
    // Content never follows the phase: rows 1-2 are slot N, rows 3-4 slot N+1.
    expect(PatternEditPage::gridSourceSlot(0, true, 1, 2) == 2, "phase 1: rows 1-2 source is slot N");
    expect(PatternEditPage::gridSourceSlot(1, true, 1, 2) == 3, "phase 1: rows 3-4 source is slot N+1");
    expect(PatternEditPage::gridSourceSlot(0, true, 0, 2) == 2, "phase 0: rows 1-2 source is slot N");
    expect(PatternEditPage::gridSourceSlot(0, false, -1, 2) == 2, "pair off: source is the selected slot");
    expect(&steps.slotNPattern() == &engine.sceneManager().getSynthPattern(0, 2),
           "phase 1: slotNPattern is slot 2, not the sounding slot 3");
    expect(&engine.activeSynthPattern(0) == &engine.sceneManager().getSynthPattern(0, 3),
           "engine still sounds slot N+1 in phase 1");
    steps.togglePairView();
    expect(steps.playheadPair() == 0, "single view: playhead stays in rows 1-2");
    steps.togglePairView();
    engine.cancelPairPlay(0);
    expect(steps.playheadPair() == 0, "pair play stopped: normal playhead");
  }

  // 2. Refusal follows the sounding source, not the stored slot kind.
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, 2);
    const int bank = engine.current303BankIndex(0);
    const bool set = GroovePuterMaterial::setResidentKind(
        engine.sceneManager_.currentScene(), 0,
        bank * Bank<SynthPattern>::kPatterns + 3,
        GroovePuterMaterial::MaterialKind::Melody);
    expect(set, "mark slot 3 as Melody");
    // Stored kind Melody in N+1 and N, but STEPS sounds: pair play starts.
    GroovePuterMaterial::setResidentKind(
        engine.sceneManager_.currentScene(), 0,
        bank * Bank<SynthPattern>::kPatterns + 2,
        GroovePuterMaterial::MaterialKind::Melody);
    engine.sceneManager().currentScene().synthABanks[bank].patterns[2].steps[0].note = 40;
    expect(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern,
           "source is Pattern");
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::Started,
           "stored Melody kind does not refuse while Pattern sounds");
    expect(engine.pairPlayActive(0) && engine.pairPlayPhase(0) == 0,
           "started at phase 0");
    engine.cancelPairPlay(0);
    // A Melody that sounds (source Phrase) refuses.
    engine.activeMaterial_[0].kind = GroovePuterMaterial::MaterialKind::Melody;
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::NotSteps,
           "sounding Melody refuses");
    expect(!engine.pairPlayActive(0), "no state after refusal");
  }

  // 3. Slot 8 has no next slot; a queued GO refuses.
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, 7);
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::NoNextSlot,
           "slot 8 refuses");
    engine.set303PatternIndex(0, 1);
    engine.goQueued_[0] = true;
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::GoQueued,
           "queued GO refuses");
  }

  // 4. Pair play plays scene content of the next slot: notes written into
  // slot N+1 behind the event cache are published when pair play starts.
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, 2);
    const int bank = engine.current303BankIndex(0);
    // Block 2 marked slot 3 as Melody on shared resident state: undo it.
    (void)GroovePuterMaterial::setResidentKind(
        engine.sceneManager_.currentScene(), 0,
        bank * Bank<SynthPattern>::kPatterns + 3,
        GroovePuterMaterial::MaterialKind::Pattern);
    engine.sceneManager().currentScene().synthABanks[bank].patterns[3].steps[6].note = 52;
    NullGfx gfx;
    SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
    page.setBoundaries(Rect{0, 16, 240, 93});
    page.onEnter(0);
    plainL(page);
    expect(engine.pairPlayActive(0), "pair play starts with an unrefreshed N+1");
    engine.start();
    engine.processSequencerEvents(0u);
    engine.processSequencerEvents(384u);
    expect(engine.pairPlayPhase(0) == 1, "phase flips to N+1 after one bar");
    const auto& ev = engine.activePatternRuntimeEvents(0);
    expect(ev.eventForSourceStep(6) != nullptr &&
               ev.eventForSourceStep(6)->note == 52,
           "slot N+1 plays its scene note at step 6");
  }

  // 5. The refresh behind a Song cell commit (same call the commit lambda
  // makes; SongPage itself cannot be driven from this host test).
  {
    MiniAcid engine(44100.0f, nullptr);
    engine.init();
    engine.setSongMode(false);
    const int bank = engine.current303BankIndex(0);
    engine.sceneManager().currentScene().synthABanks[bank].patterns[5].steps[2].note = 47;
    engine.rebuildPatternRuntimeEventBank();
    engine.sceneManager().currentScene().synthABanks[bank].patterns[6].steps[4].note = 49;
    expect(engine.patternRuntimeBank_.select(0, bank, 6).count == 0,
           "written slot is stale before refresh");
    expect(engine.refreshPatternRuntimeEvents(0, bank, 6),
           "refresh of a written slot succeeds");
    expect(engine.patternRuntimeBank_.select(0, bank, 6).count == 1, "refreshed slot has its event");
  }

  if (!g_ok) return 1;
  std::puts("PAIR PLAY PASS");
  return 0;
}
