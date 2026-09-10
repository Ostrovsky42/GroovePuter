#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/pages/tb303_params_page.h"
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

UIEvent letterEvent(char key, KeyScanCode scancode, bool alt = false) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  event.scancode = scancode;
  event.alt = alt;
  return event;
}

void enterPhrase(MiniAcid& engine, SynthSequencerPage& page) {
  (void)page;
  expect(engine.makePhrase(0), "MAKE PHRASE failed in fixture");
  expect(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase,
         "fixture did not enter Phrase source");
}

void testPhysicalLengthKeyChangesAuthoritativePhraseExtent() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  enterPhrase(engine, page);
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

  expect(page.handleEvent(length), "second physical L was not consumed");
  expect(engine.currentPhraseBuffer(0).lengthTicks ==
             static_cast<uint16_t>(4u * PhraseRuntime::kTicksPerBar),
         "physical L did not change authoritative Phrase length 2 -> 4");
  expect(page.handleEvent(length), "third physical L was not consumed");
  expect(engine.currentPhraseBuffer(0).lengthTicks ==
             static_cast<uint16_t>(8u * PhraseRuntime::kTicksPerBar),
         "physical L did not change authoritative Phrase length 4 -> 8");
}

void testCrossBoundaryEventDoesNotBlockExpansion() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  enterPhrase(engine, page);

  auto& phrase = engine.currentPhraseBuffer(0);
  phrase = PhraseRuntime::RuntimeSynthEventBuffer{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0].startTick =
      static_cast<uint16_t>(PhraseRuntime::kTicksPerBar - 12u);
  phrase.events[0].durationSubticks =
      static_cast<uint16_t>(24u * PhraseRuntime::kSubticksPerTick);
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;

  UIEvent length = letterEvent('l', GROOVEPUTER_L);
  expect(page.handleEvent(length),
         "L was not consumed for a Phrase with a cross-boundary event");
  expect(engine.currentPhraseBuffer(0).lengthTicks ==
             static_cast<uint16_t>(2u * PhraseRuntime::kTicksPerBar),
         "cross-boundary event was incorrectly validated against OLD extent");
  expect(engine.currentPhraseBuffer(0).count == 1,
         "expansion deleted the cross-boundary event");
  expect(engine.currentPhraseBuffer(0).events[0].startTick ==
             static_cast<uint16_t>(PhraseRuntime::kTicksPerBar - 12u),
         "expansion moved the cross-boundary event");
}

void testShrinkPolicyRejectsTruncationButAllowsSafeShrink() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  enterPhrase(engine, page);

  auto& phrase = engine.currentPhraseBuffer(0);
  phrase = PhraseRuntime::RuntimeSynthEventBuffer{};
  phrase.lengthTicks = static_cast<uint16_t>(2u * PhraseRuntime::kTicksPerBar);
  phrase.count = 1;
  phrase.events[0].startTick =
      static_cast<uint16_t>(PhraseRuntime::kTicksPerBar + 24u);
  phrase.events[0].durationSubticks =
      static_cast<uint16_t>(12u * PhraseRuntime::kSubticksPerTick);
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;

  UIEvent shrink = letterEvent('l', GROOVEPUTER_L, true);
  expect(page.handleEvent(shrink), "Alt+L shrink gesture was not consumed");
  expect(engine.currentPhraseBuffer(0).lengthTicks ==
             static_cast<uint16_t>(2u * PhraseRuntime::kTicksPerBar),
         "shrink silently truncated material outside requested extent");
  expect(engine.currentPhraseBuffer(0).count == 1,
         "rejected shrink deleted an event");

  phrase.count = 0;
  expect(page.handleEvent(shrink), "safe Alt+L shrink was not consumed");
  expect(engine.currentPhraseBuffer(0).lengthTicks == PhraseRuntime::kTicksPerBar,
         "safe 2 -> 1 shrink was rejected");
}

void testScancodeOnlyLengthKeyStillWorks() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);
  enterPhrase(engine, page);
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
  enterPhrase(engine, page);

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
  enterPhrase(engine, page);

  UIEvent grid = letterEvent(0, GROOVEPUTER_G);
  expect(page.handleEvent(grid), "scancode-only G was not consumed");
  gfx.clear(IGfxColor{});
  page.draw(gfx);
  expect(drewTextContaining(gfx, "GRID 1/32"),
         "scancode-only G did not change GRID");
}

void testMoreMakePhraseMaterializesPattern() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  TB303ParamsPage page(gfx, engine, AudioGuard{}, 0);
  page.setBoundaries(Rect{Layout::CONTENT.x, Layout::CONTENT.y,
                          Layout::CONTENT.w, Layout::CONTENT.h});
  page.showMoreTab(true);
  page.draw(gfx);

  UIEvent down = letterEvent(0, GROOVEPUTER_DOWN);
  for (int row = 0; row < 6; ++row) {
    expect(page.handleEvent(down), "MORE Down navigation was not consumed");
  }
  UIEvent activate = letterEvent(0, GROOVEPUTER_RIGHT);
  expect(page.handleEvent(activate), "MAKE PHRASE activation was not consumed");
  expect(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase,
         "MAKE PHRASE row did not select Phrase");
}
}  // namespace

int main() {
  testPhysicalLengthKeyChangesAuthoritativePhraseExtent();
  testCrossBoundaryEventDoesNotBlockExpansion();
  testShrinkPolicyRejectsTruncationButAllowsSafeShrink();
  testScancodeOnlyLengthKeyStillWorks();
  testGridCyclesOnRepeatedPhysicalG();
  testScancodeOnlyGridKeyCycles();
  testMoreMakePhraseMaterializesPattern();

  if (failures == 0) {
    std::printf("Pattern/Phrase hardware controls: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "Pattern/Phrase hardware controls: %d failure(s)\n", failures);
  return 1;
}
