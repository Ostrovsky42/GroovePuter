// DRUMS Ctrl+Backspace (clear pattern), Alt+Backspace, Backspace, STEP SETTINGS
// Backspace resets, and the "has settings" dot on grid cells. Real page, real
// engine, real Undo owner.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"
#define private public
#include "src/ui/pages/drum_sequencer_page.h"
#undef private
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

class CtrlBkspGfx final : public IGfx {
 public:
  std::string text;
  std::set<std::pair<int, int>> dots;  // top-left of every 2x2 fill
  int dotCount = 0;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* value) override {
    if (value) text += value;
  }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int x, int y, int w, int h, IGfxColor) override {
    if (w == 2 && h == 2 && x >= 20 && y >= 20) {  // inside the grid area
      dots.insert({x, y});
      ++dotCount;
    }
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

static UIEvent keyEvent(int key, bool ctrl = false, bool alt = false) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = static_cast<char>(key);
  e.ctrl = ctrl;
  e.alt = alt;
  return e;
}

static bool samePattern(const DrumPatternSet& a, const DrumPatternSet& b) {
  return std::memcmp(&a, &b, sizeof(DrumPatternSet)) == 0;
}

static bool hitsClear(const DrumPatternSet& p) {
  for (int v = 0; v < NUM_DRUM_VOICES; ++v)
    for (int s = 0; s < SEQ_STEPS; ++s)
      if (p.voices[v].steps[s].hit || p.voices[v].steps[s].accent) return false;
  return true;
}

int main() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  engine.setSongMode(false);
  SceneManager& manager = engine.sceneManager();
  const int bank = manager.getCurrentBankIndex(0);
  const int slot = manager.getCurrentDrumPatternIndex();
  const int other = (slot + 1) % 8;
  DrumPatternSet& cur = manager.currentScene().drumBanks[bank].patterns[slot];
  DrumPatternSet& neighbour = manager.currentScene().drumBanks[bank].patterns[other];

  const auto fill = [](DrumPatternSet& p) {
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
      for (int s = 0; s < SEQ_STEPS; ++s) {
        DrumStep& st = p.voices[v].steps[s];
        st.hit = ((v + s) % 3) == 0;
        st.accent = st.hit && (s % 4 == 0);
        st.probability = static_cast<uint8_t>(60 + v);
        st.timing = static_cast<int8_t>(s - 8);
        st.velocity = static_cast<uint8_t>(70 + s);
        st.fx = (s % 5 == 0) ? DRUM_FX_FLAM : DRUM_FX_NONE;
        st.fxParam = st.fx ? 2 : 0;
      }
    }
  };
  fill(cur);
  fill(neighbour);
  neighbour.voices[3].steps[3].hit = true;
  const DrumPatternSet neighbourBefore = neighbour;
  GroovePuterUndo::undoOwner().clear();

  CtrlBkspGfx gfx;
  DrumSequencerPage page(gfx, engine, AudioGuard{});
  page.setBoundaries(Rect{0, 0, 240, 135});
  page.onEnter(0);

  const auto toast = [&]() {
    gfx.text.clear();
    UI::drawToast(gfx);
    return gfx.text;
  };
  const auto undo = [&]() {
    UIEvent u{};
    u.event_type = GROOVEPUTER_APPLICATION_EVENT;
    u.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    return page.handleEvent(u);
  };

  // (c) Alt+Backspace: no change, handled, tells where the clear lives.
  const DrumPatternSet original = cur;
  {
    UIEvent e = keyEvent('\b', false, true);
    assert(page.handleEvent(e));
    assert(samePattern(cur, original));
    assert(toast().find("CTRL+BKSP CLEARS PATTERN") != std::string::npos);
  }

  // Focus the grid (Down then Up returns to voice 0).
  for (KeyScanCode nav : {GROOVEPUTER_DOWN, GROOVEPUTER_UP}) {
    UIEvent arrow{};
    arrow.event_type = GROOVEPUTER_KEY_DOWN;
    arrow.scancode = nav;
    assert(page.handleEvent(arrow));
  }
  // (d) Backspace clears exactly one cell (hit + accent), nothing else.
  // The cursor starts on voice 0, step 0: (0+0)%3==0, a hit with accent.
  assert(cur.voices[0].steps[0].hit && cur.voices[0].steps[0].accent);
  {
    UIEvent e = keyEvent('\b');
    assert(page.handleEvent(e));
    DrumPatternSet expect = original;
    expect.voices[0].steps[0].hit = false;
    expect.voices[0].steps[0].accent = false;
    assert(samePattern(cur, expect));
    assert(undo());
    assert(samePattern(cur, original));
  }

  // (a)+(e) Ctrl+Backspace without selection: every hit/accent of this slot
  // goes, every other field stays, the neighbour slot is untouched.
  {
    UIEvent e = keyEvent('\b', true);
    assert(page.handleEvent(e));
    assert(hitsClear(cur));
    for (int v = 0; v < NUM_DRUM_VOICES; ++v)
      for (int s = 0; s < SEQ_STEPS; ++s) {
        const DrumStep& a = cur.voices[v].steps[s];
        const DrumStep& b = original.voices[v].steps[s];
        assert(a.probability == b.probability && a.timing == b.timing &&
               a.velocity == b.velocity && a.fx == b.fx && a.fxParam == b.fxParam);
      }
    assert(samePattern(neighbour, neighbourBefore));
    assert(toast().find("DRUMS CLEARED  CTRL+Z UNDO") != std::string::npos);
    // (b) Undo restores byte for byte.
    assert(undo());
    assert(samePattern(cur, original));
    assert(samePattern(neighbour, neighbourBefore));
  }

  // Already clear: one honest toast, no Undo step added.
  {
    UIEvent e = keyEvent('\b', true);
    assert(page.handleEvent(e));
    assert(hitsClear(cur));
    UIEvent again = keyEvent('\b', true);
    assert(page.handleEvent(again));
    assert(toast().find("DRUMS ALREADY CLEAR") != std::string::npos);
    assert(undo());
    assert(samePattern(cur, original));
  }

  // (g) A-1: with a selection Ctrl+Backspace clears the WHOLE pattern (as on
  // SYNTH) and drops the selection.
  {
    UIEvent right{};
    right.event_type = GROOVEPUTER_KEY_DOWN;
    right.scancode = GROOVEPUTER_RIGHT;
    right.ctrl = true;
    for (int i = 0; i < 3; ++i) assert(page.handleEvent(right));  // steps 0..3
    UIEvent e = keyEvent('\b', true);
    assert(page.handleEvent(e));
    assert(hitsClear(cur));
    assert(toast().find("DRUMS CLEARED  CTRL+Z UNDO") != std::string::npos);
    assert(undo());
    assert(samePattern(cur, original));
    // Selection is gone: plain Backspace clears only the cursor cell (0,3),
    // not the old 0..3 area (which holds hits at steps 0 and 3).
    UIEvent bk = keyEvent('\b');
    assert(page.handleEvent(bk));
    DrumPatternSet expect = original;
    expect.voices[0].steps[3].hit = false;
    expect.voices[0].steps[3].accent = false;
    assert(samePattern(cur, expect));
    assert(undo());
    assert(samePattern(cur, original));
    // Back to step 0 for the next case.
    UIEvent left{};
    left.event_type = GROOVEPUTER_KEY_DOWN;
    left.scancode = GROOVEPUTER_LEFT;
    for (int i = 0; i < 3; ++i) assert(page.handleEvent(left));
  }

  // (f) STEP SETTINGS open: Ctrl+Backspace clears no hit; it resets every
  // field of the step. Backspace resets only the field under the cursor.
  {
    UIEvent alt_e = keyEvent('e', false, true);
    assert(page.handleEvent(alt_e));
    UIEvent down{};
    down.event_type = GROOVEPUTER_KEY_DOWN;
    down.scancode = GROOVEPUTER_DOWN;
    assert(page.handleEvent(down));  // PROB -> TIMING
    UIEvent bk = keyEvent('\b');
    assert(page.handleEvent(bk));
    {
      DrumPatternSet expect = original;
      expect.voices[0].steps[0].timing = 0;
      assert(samePattern(cur, expect));
    }
    assert(undo());
    assert(samePattern(cur, original));
    UIEvent ck = keyEvent('\b', true);
    assert(page.handleEvent(ck));
    {
      DrumPatternSet expect = original;
      DrumStep& st = expect.voices[0].steps[0];
      st.probability = 100;
      st.timing = 0;
      st.velocity = 100;
      st.fx = DRUM_FX_NONE;
      st.fxParam = 0;
      assert(samePattern(cur, expect));
    }
    assert(undo());
    assert(samePattern(cur, original));
    // Hits are untouched by both resets (checked above); the window is still
    // open, so Ctrl+Backspace has not cleared the pattern. Close it: Backspace
    // is the cell clear again.
    UIEvent esc{};
    esc.event_type = GROOVEPUTER_KEY_DOWN;
    esc.scancode = GROOVEPUTER_ESCAPE;
    assert(page.handleEvent(esc));
    UIEvent after = keyEvent('\b');
    assert(page.handleEvent(after));
    assert(!cur.voices[0].steps[0].hit);
    assert(undo());
    assert(samePattern(cur, original));
  }

  // A-7: a hit cell with PROB, VEL or FLAM off default gets one extra 2x2 dot,
  // and it never lands on the TIMING mark.
  {
    DrumPatternSet& p = cur;
    for (int v = 0; v < NUM_DRUM_VOICES; ++v)
      for (int s = 0; s < SEQ_STEPS; ++s) p.voices[v].steps[s] = DrumStep{};
    p.voices[0].steps[0].hit = true;
    const auto drawDots = [&]() {
      gfx.dots.clear();
      gfx.dotCount = 0;
      page.draw(gfx);
      return gfx.dots;
    };
    const auto base = drawDots();
    p.voices[0].steps[0].probability = 50;
    const auto prob = drawDots();
    assert(prob.size() == base.size() + 1);
    p.voices[0].steps[0].probability = 100;
    p.voices[0].steps[0].velocity = 80;
    assert(drawDots().size() == base.size() + 1);
    p.voices[0].steps[0].velocity = 100;
    p.voices[0].steps[0].fx = DRUM_FX_ROLL;
    assert(drawDots().size() == base.size() + 1);
    p.voices[0].steps[0].fx = DRUM_FX_NONE;
    p.voices[0].steps[0].timing = 3;
    const auto timing = drawDots();
    assert(timing.size() == base.size() + 1);  // TIMING alone: its own mark only
    p.voices[0].steps[0].probability = 50;
    const auto both = drawDots();
    assert(both.size() == base.size() + 2);  // two distinct squares, no overlap
  }

  std::printf("drum ctrl+backspace tests passed\n");
  return 0;
}
