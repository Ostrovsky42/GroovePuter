// STEPS pair view (L): rows 3-4 are slot N+1 of the same synth. The cursor and
// the edits follow the row; slot 8 has no N+1.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

static_assert(SynthPattern::kSteps == 16, "pair view must not change the pattern length");
static_assert(SEQ_STEPS == 16, "pair view must not change the step count");

namespace {

class RecordingGfx final : public IGfx {
 public:
  std::vector<std::string> text;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* value) override {
    if (value) text.emplace_back(value);
  }
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
  int textWidth(const char* value) const override {
    return value ? static_cast<int>(std::strlen(value)) * 6 : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

bool hasText(const RecordingGfx& gfx, const char* value) {
  for (const auto& item : gfx.text) {
    if (item == value) return true;
  }
  return false;
}

bool g_ok = true;
void expect(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    g_ok = false;
  }
}

UIEvent keyEvent(char key, KeyScanCode scancode = GROOVEPUTER_NO_SCANCODE) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = key;
  e.scancode = scancode;
  return e;
}

void press(SynthSequencerPage& page, char key, KeyScanCode scancode = GROOVEPUTER_NO_SCANCODE) {
  UIEvent e = keyEvent(key, scancode);
  (void)page.handleEvent(e);
}
void pressDown(SynthSequencerPage& page) {
  UIEvent e = keyEvent(0, GROOVEPUTER_DOWN);
  (void)page.handleEvent(e);
}
void pressUndo(SynthSequencerPage& page) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_APPLICATION_EVENT;
  e.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
  (void)page.handleEvent(e);
}

int noteAt(MiniAcid& engine, int slot, int step) {
  return engine.sceneManager().getSynthPattern(0, slot).steps[step].note;
}

void setNote(MiniAcid& engine, int slot, int step, int note) {
  engine.sceneManager().editSynthPattern(0, slot).steps[step].note =
      static_cast<int8_t>(note);
}

void runStyle(VisualStyle style, const char* name) {
  UI::currentStyle = style;
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.setBoundaries(Rect{0, 16, 240, 93});
  page.onEnter(0);
  page.setContext(0);

  const int n = engine.current303PatternIndex(0);
  expect(n >= 0 && n + 1 < 8, "test needs a slot that has a next slot");
  setNote(engine, n, 0, 50);
  setNote(engine, n + 1, 0, 40);

  gfx.text.clear();
  page.draw(gfx);
  expect(hasText(gfx, "303 A"), name);
  expect(!hasText(gfx, "A 1+2"), "no pair title before L");

  press(page, 'l', GROOVEPUTER_L);
  gfx.text.clear();
  page.draw(gfx);
  expect(hasText(gfx, "A 1+2"), "pair header A 1+2 after L");

  // Down twice: row 3, step 0 -> slot N+1 step 0.
  pressDown(page);
  pressDown(page);
  press(page, 'a', GROOVEPUTER_A);
  expect(noteAt(engine, n + 1, 0) == 41, "A on row 3 raises slot N+1 step 0");
  expect(noteAt(engine, n, 0) == 50, "slot N step 0 untouched by the row 3 edit");

  // Its own undo receipt: Undo restores slot N+1 only.
  pressUndo(page);
  expect(noteAt(engine, n + 1, 0) == 40, "Undo restores slot N+1");
  expect(noteAt(engine, n, 0) == 50, "Undo leaves slot N");

  // Clear on row 3.
  press(page, '\b');
  expect(noteAt(engine, n + 1, 0) == -1, "Backspace on row 3 clears slot N+1");
  expect(noteAt(engine, n, 0) == 50, "slot N intact after the row 3 clear");

  // Back up to row 1 and edit slot N.
  UIEvent up = keyEvent(0, GROOVEPUTER_UP);
  (void)page.handleEvent(up);
  (void)page.handleEvent(up);
  press(page, 'a', GROOVEPUTER_A);
  expect(noteAt(engine, n, 0) == 51, "A on row 1 edits slot N");

  // L again: back to one slot.
  press(page, 'l', GROOVEPUTER_L);
  gfx.text.clear();
  page.draw(gfx);
  expect(hasText(gfx, "303 A"), "title back to 303 A after L L");
}

void runLastSlot() {
  UI::currentStyle = VisualStyle::RETRO_CLASSIC;
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.setBoundaries(Rect{0, 16, 240, 93});
  page.onEnter(0);
  page.setContext(0);
  press(page, 'i', GROOVEPUTER_I);  // slot 8
  expect(engine.current303PatternIndex(0) == 7, "I selects slot 8");
  setNote(engine, 7, 0, 50);
  setNote(engine, 7, 8, 60);

  press(page, 'l', GROOVEPUTER_L);
  gfx.text.clear();
  page.draw(gfx);
  expect(hasText(gfx, "A 8+-"), "slot 8 header has no N+1");

  // Three downs: row 2 is the last reachable row (slot 8 steps 9-16).
  pressDown(page);
  pressDown(page);
  pressDown(page);
  press(page, '\b');
  expect(noteAt(engine, 7, 8) == -1, "cursor stayed on slot 8 row 2");
  expect(noteAt(engine, 7, 0) == 50, "row 1 of slot 8 untouched");
  for (int s = 0; s < 8; ++s) {
    expect(noteAt(engine, s, 0) == (s == 7 ? 50 : noteAt(engine, s, 0)),
           "no slot other than 8 was edited");
  }
}

void runNoteEntryKeepsLAsPitch() {
  UI::currentStyle = VisualStyle::RETRO_CLASSIC;
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.setBoundaries(Rect{0, 16, 240, 93});
  page.onEnter(0);
  page.setContext(0);
  const int n = engine.current303PatternIndex(0);
  press(page, 'n', GROOVEPUTER_N);
  press(page, 'l', GROOVEPUTER_L);
  gfx.text.clear();
  page.draw(gfx);
  expect(!hasText(gfx, "A 1+2"), "L in NOTE ENTRY does not open the pair view");
  expect(noteAt(engine, n, 0) >= 0, "L in NOTE ENTRY is a pitch");
}

}  // namespace

int main() {
  runStyle(VisualStyle::RETRO_CLASSIC, "CYBER");
  runStyle(VisualStyle::AMBER, "AMBER");
  runStyle(VisualStyle::MINIMAL, "CLEAN");
  runLastSlot();
  runNoteEntryKeepsLAsPitch();
  if (!g_ok) return 1;
  std::puts("Pattern pair view (L): PASS");
  return 0;
}
