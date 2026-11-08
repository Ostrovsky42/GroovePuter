#include <cassert>
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"
#include "src/ui/miniacid_display.h"

SerialMock Serial;
SDMock SD;

class TestGfx final : public IGfx {
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
  int textWidth(const char* s) const override { return s ? int(std::strlen(s)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

int main() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  engine.setSongMode(false);
  engine.set303PatternIndex(0, 0);
  auto& bank = engine.sceneManager().currentScene().synthABanks[0];
  bank.patterns[0] = SynthPattern{};
  bank.patterns[1] = SynthPattern{};
  bank.patterns[0].steps[0].note = 48;
  bank.patterns[1].steps[0].note = 60;
  assert(engine.rebuildPatternRuntimeEventBank());
  TestGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay ui(gfx, engine, keyboard);
  ui.dismissSplash();
  ui.goToPage(1);
  engine.start();
  engine.processSequencerEvents(0);
  UIEvent pair{};
  pair.event_type = GROOVEPUTER_KEY_DOWN;
  pair.key = 'l';
  pair.scancode = GROOVEPUTER_L;
  assert(ui.handleEvent(pair));
  assert(engine.pairPlayActive(0));
  engine.processSequencerEvents(384);
  assert(engine.patternPlaybackState_[0].activeNote() == 48);
  ui.goToPage(5);
  ui.update();
  engine.processSequencerEvents(768);
  assert(engine.pairPlayActive(0));
  assert(engine.patternPlaybackState_[0].activeNote() == 60);
  ui.goToPage(2);
  ui.update();
  engine.processSequencerEvents(1152);
  assert(engine.patternPlaybackState_[0].activeNote() == 48);
  ui.goToPage(1);
  ui.update();
  engine.processSequencerEvents(1536);
  assert(engine.patternPlaybackState_[0].activeNote() == 60);
  engine.stop();
  assert(engine.pairPlayActive(0));
  ui.goToPage(5);
  ui.goToPage(1);
  engine.start();
  engine.processSequencerEvents(0);
  assert(engine.patternPlaybackState_[0].activeNote() == 48);
  engine.processSequencerEvents(384);
  assert(engine.patternPlaybackState_[0].activeNote() == 60);
  engine.stop();
  std::puts("Pair playback survives real UI navigation: PASS");
}
