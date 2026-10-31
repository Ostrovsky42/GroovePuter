// STEPS PAIR PLAY (Ctrl+L): one voice loops slot N, N+1 on the bar line. It is
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

void ctrlL(SynthSequencerPage& page) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = 'l';
  e.scancode = GROOVEPUTER_L;
  e.ctrl = true;
  (void)page.handleEvent(e);
}

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

    ctrlL(page);
    expect(engine.pairPlayActive(0), "Ctrl+L starts pair play");
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

    ctrlL(page);
    expect(!engine.pairPlayActive(0), "Ctrl+L again stops the pair");
    expect(patA(engine) == 2, "stopped pair returns to the selected slot");

    // Cancel on Stop.
    ctrlL(page);
    expect(engine.pairPlayActive(0), "restart");
    engine.stop();
    expect(!engine.pairPlayActive(0), "transport Stop cancels");

    // Cancel on slot change and on leaving the page.
    ctrlL(page);
    engine.set303PatternIndex(0, 4);
    expect(!engine.pairPlayActive(0), "changing slot cancels");
    engine.set303PatternIndex(0, 2);
    ctrlL(page);
    expect(engine.pairPlayActive(0), "restart before leaving");
    page.onExit();
    expect(!engine.pairPlayActive(0), "leaving the page cancels");

    // Song on cancels and refuses.
    ctrlL(page);
    engine.setSongMode(true);
    expect(!engine.pairPlayActive(0), "Song on cancels");
    ctrlL(page);
    expect(!engine.pairPlayActive(0), "Song on refuses");
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::SongOn,
           "Song on reports SongOn");
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
    ctrlL(host);
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
    steps.togglePairView();
    expect(steps.playheadPair() == 0, "single view: playhead stays in rows 1-2");
    steps.togglePairView();
    engine.cancelPairPlay(0);
    expect(steps.playheadPair() == 0, "pair play stopped: normal playhead");
  }

  // 2. A Melody slot in the pair refuses.
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
    expect(engine.togglePairPlay(0) == MiniAcid::PairPlayResult::NotSteps,
           "Melody in N+1 refuses");
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

  if (!g_ok) return 1;
  std::puts("PAIR PLAY PASS");
  return 0;
}
