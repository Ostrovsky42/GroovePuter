// PROJECT -> LED: the brightness ladder reaches full scale (it used to stop at raw 90 = 35%).
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/platform/cardputer_usb_role_runtime.h"
#include "src/state/scene_revision.h"
#include "src/ui/pages/project_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {
class NullGfx : public IGfx {
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
  int textWidth(const char* t) const override { return t ? static_cast<int>(std::strlen(t)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

UIEvent key(char k) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = k;
  return e;
}
UIEvent scan(KeyScanCode c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = c;
  return e;
}
}  // namespace

int main() {
  MiniAcid engine(44100.0f, nullptr);
  NullGfx gfx;
  ProjectPage page(gfx, engine, AudioGuard{});
  page.onEnter(0);
  auto send = [&](UIEvent e) { (void)page.handleEvent(e); };
  auto& led = engine.sceneManager().currentScene().led;

  send(key('\t'));
  send(key('\t'));  // Scenes -> Groove -> Led: first row is the mode
  for (int i = 0; i < 3; ++i) send(scan(GROOVEPUTER_DOWN));  // mode, source, colour, brightness

  led.brightness = 90;  // the old ceiling
  send(scan(GROOVEPUTER_RIGHT));
  assert(led.brightness > 90);
  uint8_t highest = led.brightness;
  for (int i = 0; i < 8; ++i) {
    send(scan(GROOVEPUTER_RIGHT));
    if (led.brightness > highest) highest = led.brightness;
  }
  assert(highest == 255);  // full scale is reachable

  led.brightness = 255;
  send(scan(GROOVEPUTER_RIGHT));
  assert(led.brightness == 10);  // wraps to the first step
  send(scan(GROOVEPUTER_LEFT));
  assert(led.brightness == 255);  // and back to the top

  std::puts("PROJECT LED brightness ladder reaches full scale: PASS");
  return 0;
}
