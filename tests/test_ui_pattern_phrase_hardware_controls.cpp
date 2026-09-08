#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/screen_geometry.h"

SerialMock Serial;
SDMock SD;

namespace {
constexpr float kTestSampleRate = 44100.0f;

class RecordingGfx : public IGfx {
 public:
  struct TextEntry {
    int x;
    int y;
    std::string text;
  };
  std::vector<TextEntry> texts;

  void begin() override {}
  void clear(IGfxColor) override { texts.clear(); }
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int x, int y, const char* text) override {
    if (text && text[0] != '\0') texts.push_back({x, y, text});
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
  int textWidth(const char* text) const override {
    return text ? static_cast<int>(std::strlen(text)) * 6 : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

int failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "Pattern/Phrase hardware controls FAIL: %s\n", message);
  ++failures;
}

bool drewTextContaining(const RecordingGfx& gfx, const char* needle) {
  for (const auto& entry : gfx.texts) {
    if (entry.text.find(needle) != std::string::npos) return true;
  }
  return false;
}

UIEvent letterEvent(char key, KeyScanCode scancode) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  event.scancode = scancode;
  return event;
}

void testPhysicalLengthKeyChangesAuthoritativePhraseExtent() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  expect(engine.makePhrase(0), "MAKE PHRASE failed in fixture");
  expect(engine.setPhraseLength(0, 1), "fixture could not force 1-bar Phrase");

  UIEvent length = letterEvent('l', GROOVEPUTER_L);
  expect(page.handleEvent(length), "plain physical L was not consumed by Phrase page");
  expect(engine.currentPhraseBuffer(0).lengthTicks ==
             static_cast<uint16_t>(2u * PhraseRuntime::kTicksPerBar),
         "plain physical L did not change authoritative Phrase length 1 -> 2");

  gfx.clear(IGfxColor{});
  page.draw(gfx);
  expect(drewTextContaining(gfx, "BAR 1/2"),
         "Phrase header did not render BAR 1/2 after accepted length change");
}

void testScancodeOnlyLengthKeyStillWorks() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  expect(engine.makePhrase(0), "MAKE PHRASE failed in scancode fixture");
  expect(engine.setPhraseLength(0, 1), "scancode fixture could not force 1 bar");

  UIEvent length = letterEvent(0, GROOVEPUTER_L);
  expect(page.handleEvent(length), "scancode-only L was not consumed by Phrase page");
  expect(engine.currentPhraseBuffer(0).lengthTicks ==
             static_cast<uint16_t>(2u * PhraseRuntime::kTicksPerBar),
         "scancode-only L did not change authoritative Phrase length");
}

void testGridCyclesOnRepeatedPhysicalG() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  expect(engine.makePhrase(0), "MAKE PHRASE failed in GRID fixture");

  UIEvent grid = letterEvent('g', GROOVEPUTER_G);
  expect(page.handleEvent(grid), "first physical G was not consumed");
  gfx.clear(IGfxColor{});
  page.draw(gfx);
  expect(drewTextContaining(gfx, "GRID 1/32"),
         "first G did not move GRID 1/16 -> 1/32");

  expect(page.handleEvent(grid), "second physical G was not consumed");
  gfx.clear(IGfxColor{});
  page.draw(gfx);
  expect(drewTextContaining(gfx, "GRID 1/8"),
         "second G did not wrap GRID 1/32 -> 1/8");

  expect(page.handleEvent(grid), "third physical G was not consumed");
  gfx.clear(IGfxColor{});
  page.draw(gfx);
  expect(drewTextContaining(gfx, "GRID 1/16"),
         "third G did not advance GRID 1/8 -> 1/16");
}

void testScancodeOnlyGridKeyCycles() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  expect(engine.makePhrase(0), "MAKE PHRASE failed in GRID scancode fixture");

  UIEvent grid = letterEvent(0, GROOVEPUTER_G);
  expect(page.handleEvent(grid), "scancode-only G was not consumed");
  gfx.clear(IGfxColor{});
  page.draw(gfx);
  expect(drewTextContaining(gfx, "GRID 1/32"),
         "scancode-only G did not change GRID");
}
}  // namespace

int main() {
  testPhysicalLengthKeyChangesAuthoritativePhraseExtent();
  testScancodeOnlyLengthKeyStillWorks();
  testGridCyclesOnRepeatedPhysicalG();
  testScancodeOnlyGridKeyCycles();

  if (failures == 0) {
    std::printf("Pattern/Phrase hardware controls: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "Pattern/Phrase hardware controls: %d failure(s)\n", failures);
  return 1;
}
