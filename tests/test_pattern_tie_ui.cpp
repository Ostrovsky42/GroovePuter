#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

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

bool checkTheme(VisualStyle style, const char* name) {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  RecordingGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.setBoundaries(Rect{0, 16, 240, 93});
  page.onEnter(0);
  page.setContext(1);
  engine.sceneManager().editCurrentSynthPattern(0).steps[1].note = -1;
  UI::currentStyle = style;
  page.draw(gfx);
  if (hasText(gfx, "TI")) {
    std::fprintf(stderr, "%s: REST is incorrectly displayed as TI\n", name);
    return false;
  }
  gfx.text.clear();

  UIEvent z{};
  z.event_type = GROOVEPUTER_KEY_DOWN;
  z.key = 'z';
  z.scancode = GROOVEPUTER_Z;
  if (!page.handleEvent(z)) {
    std::fprintf(stderr, "%s: Z was not handled\n", name);
    return false;
  }
  if (engine.sceneManager().getCurrentSynthPattern(0).steps[1].note != -2) {
    std::fprintf(stderr, "%s: Z did not write TIE into step 2\n", name);
    return false;
  }

  page.draw(gfx);
  if (!hasText(gfx, "TI")) {
    std::fprintf(stderr, "%s: TIE is stored but not visible as TI\n", name);
    return false;
  }
  return true;
}

}  // namespace

int main() {
  const bool retro = checkTheme(VisualStyle::RETRO_CLASSIC, "CYBER");
  const bool amber = checkTheme(VisualStyle::AMBER, "AMBER");
  const bool minimal = checkTheme(VisualStyle::MINIMAL, "CARBON");
  if (!retro || !amber || !minimal) return 1;
  std::puts("Pattern TIE input/rendering across all themes: PASS");
  return 0;
}
