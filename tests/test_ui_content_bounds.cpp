// Slice 1.2: page content must stay inside CONTENT.
//
// The arithmetic that makes this a real defect:
//
//   lineY(n) = CONTENT.y + CONTENT_PAD_Y + n * LINE_HEIGHT = 16 + 2 + n*12
//   CONTENT spans y 16..108; PERFORMANCE_HUD begins at 109 and paints over.
//
//   line 6 -> y=90,  bottom 98   fits
//   line 7 -> y=102, bottom 110  does not
//
// So CONTENT holds exactly seven lines and line 7 does not exist. Thirteen
// call sites across six pages use lineY(7); the sweep showed the result as
// half-erased text under the "G1/16 TN L1B" strip on FEEL, GENRE and DRM.
//
// A source grep cannot gate this, because lineY(7) - 2 lands at y=100 and is
// perfectly legal. The bound has to be computed from what the page actually
// asked the display to draw, which is what this file does: render the real
// page through a recording IGfx and check every string it emitted.
//
// Only the bottom edge is asserted here. Text above CONTENT.y cannot yet be
// separated from the legitimate header the page draws through
// UI::drawStandardHeader, and left/right clipping (the DRUMS lane labels at
// x=0) needs its own approach; both are recorded in the UI pass plan.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/ui/pages/feel_page.h"
#include "src/ui/pages/genre_page.h"
#include "src/ui/pages/phrase_page.h"
#include "src/ui/screen_geometry.h"

// The SDL production sources are linked without sdl_main.cpp, which is
// where these live; the PHW-P1 harness does the same.
SerialMock Serial;
SDMock SD;

namespace {

constexpr float kTestSampleRate = 44100.0f;
constexpr int kGlyphAdvance = 6;

class RecordingGfx : public IGfx {
 public:
  struct TextEntry {
    int x;
    int y;
    std::string text;
  };
  std::vector<TextEntry> texts;

  void begin() override {}
  void clear(IGfxColor) override {}
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
    return text ? static_cast<int>(std::strlen(text)) * kGlyphAdvance : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

int g_failures = 0;

// One page's worth of drawing, checked against the region it owns.
void checkBottomBound(const char* pageName, const RecordingGfx& gfx) {
  const int contentTop = Layout::CONTENT.y;
  const int contentBottom = Layout::CONTENT.y + Layout::CONTENT.h;

  for (const auto& entry : gfx.texts) {
    // Text above CONTENT belongs to the shared header this page draws; only
    // the page's own region is under test here.
    if (entry.y < contentTop) continue;

    const int bottom = entry.y + gfx.fontHeight();
    if (bottom <= contentBottom) continue;

    std::fprintf(stderr,
                 "content bounds FAIL: %s drew \"%s\" at y=%d, bottom %d, "
                 "past CONTENT bottom %d -- the HUD band erases it\n",
                 pageName, entry.text.c_str(), entry.y, bottom, contentBottom);
    ++g_failures;
  }
}

}  // namespace

int main() {
  // The geometry this test exists to protect. If these change, the numbers in
  // the comment above and in the UI pass plan are stale.
  static_assert(Layout::CONTENT.y == 16, "CONTENT origin moved");
  static_assert(Layout::CONTENT.h == 93, "CONTENT height moved");
  static_assert(Layout::LINE_HEIGHT == 12, "line grid changed");

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    AudioGuard guard{};
    RecordingGfx gfx;
    FeelPage page(gfx, engine, guard);
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("FeelPage", gfx);
  }

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    GenrePage page(gfx, engine, AudioGuard{});
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("GenrePage", gfx);
  }

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    PhrasePage page(gfx, engine, AudioGuard{}, false);
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("PhrasePage", gfx);
  }

  if (g_failures == 0) {
    std::printf("UI content bounds: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI content bounds: %d failure(s)\n", g_failures);
  return 1;
}
