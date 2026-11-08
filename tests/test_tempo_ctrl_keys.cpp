// Ctrl+= / Ctrl+- move the project BPM by 1 from any page that does not own
// the key; Alt+= / Alt+- by 5. Range 10-250.
#include <cstdio>
#include <string>
#include <vector>
#include "platform_sdl/sdl_display.h"
#include "src/dsp/miniacid_engine.h"
#include "src/input/performance_keyboard.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/ui/miniacid_display.h"

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
void expect(bool c, const char* w) {
  if (!c) { std::fprintf(stderr, "FAIL: %s\n", w); g_ok = false; }
}
UIEvent combo(char k, bool ctrl, bool alt) {
  UIEvent e{}; e.event_type = GROOVEPUTER_KEY_DOWN; e.key = k; e.ctrl = ctrl; e.alt = alt; return e;
}
}  // namespace

int main() {
  MiniAcid engine(44100.f, nullptr);
  engine.init();
  NullGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  GroovePuterMidi::transportClockRuntime().setSource(
      GroovePuterMidi::TransportClockSource::GroovePuterInternal);

  engine.setBpm(120);
  display.handleEvent(combo('=', true, false));
  expect(engine.bpm() == 121, "Ctrl+= is +1");
  display.handleEvent(combo('+', true, false));
  expect(engine.bpm() == 122, "Ctrl++ is +1");
  display.handleEvent(combo('-', true, false));
  display.handleEvent(combo('-', true, false));
  expect(engine.bpm() == 120, "Ctrl+- moves back");
  display.handleEvent(combo('=', false, true));
  expect(engine.bpm() == 125, "Alt+= is +5");
  display.handleEvent(combo('-', false, true));
  expect(engine.bpm() == 120, "Alt+- is -5");

  engine.setBpm(250);
  display.handleEvent(combo('=', true, false));
  expect(engine.bpm() == 250, "clamps at 250");
  engine.setBpm(10);
  display.handleEvent(combo('-', true, false));
  expect(engine.bpm() == 10, "clamps at 10");

  // Plain = / - are not BPM keys.
  engine.setBpm(120);
  display.handleEvent(combo('=', false, false));
  expect(engine.bpm() == 120, "plain = leaves BPM alone");

  if (!g_ok) return 1;
  std::puts("TEMPO CTRL KEYS PASS");
  return 0;
}
