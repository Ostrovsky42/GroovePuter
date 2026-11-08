// PERFORM page: the Cardputer arrow keycaps (; , . /) deliver only the arrow scancode since #465,
// so the live scale selection must also answer to LEFT/RIGHT, not just to the printed characters.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"
#include "src/ui/pages/perform_page.h"
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

UIEvent arrow(KeyScanCode code) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = code;  // key stays 0: the character is suppressed centrally
  return e;
}
}  // namespace

int main() {
  MiniAcid engine(44100.0f, nullptr);
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  NullGfx gfx;
  PerformPage page(gfx, engine, keyboard);
  page.onEnter(0);

  const PerformanceScale start = keyboard.scale();
  UIEvent right = arrow(GROOVEPUTER_RIGHT);
  assert(page.handleEvent(right));
  assert(keyboard.scale() != start);
  UIEvent left = arrow(GROOVEPUTER_LEFT);
  assert(page.handleEvent(left));
  assert(keyboard.scale() == start);

  // The printed-character path still works (desktop keyboard) and does not double-step.
  UIEvent comma{};
  comma.event_type = GROOVEPUTER_KEY_DOWN;
  comma.key = '.';
  assert(page.handleEvent(comma));
  assert(keyboard.scale() != start);

  // Modified arrows are not scale steps (Fn / Alt / Ctrl combinations belong to global shortcuts).
  UIEvent alt = arrow(GROOVEPUTER_LEFT);
  alt.alt = true;
  const PerformanceScale before = keyboard.scale();
  (void)page.handleEvent(alt);
  assert(keyboard.scale() == before);

  std::puts("PERFORM live scale answers to the arrow keys: PASS");
  return 0;
}
