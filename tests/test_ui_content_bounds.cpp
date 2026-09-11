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
#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"
#include "src/ui/pages/perform_page.h"
#include "src/ui/pages/phrase_page.h"
#include "src/ui/pages/song_page.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/layout_manager.h"
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

// The y coordinates the shared header legitimately uses. Recorded from the
// real header rather than guessed, so a page drawing above CONTENT.y can be
// told apart from the header the page draws through UI::drawStandardHeader.
std::vector<int> headerRowsY() {
  MiniAcid engine(kTestSampleRate, nullptr);
  RecordingGfx gfx;
  UI::drawStandardHeader(gfx, engine, "TITLE");
  std::vector<int> rows;
  for (const auto& entry : gfx.texts) rows.push_back(entry.y);
  return rows;
}

// Content drawn above CONTENT.y is sliced by the header rule -- on SONG the
// "EDIT:A PLAY:A PAT:A ALL" line loses its top row of pixels, on DRUMS the
// pattern row does.
void checkTopBound(const char* pageName, const RecordingGfx& gfx,
                   const std::vector<int>& headerRows) {
  for (const auto& entry : gfx.texts) {
    if (entry.y >= Layout::CONTENT.y) continue;
    bool isHeader = false;
    for (int y : headerRows) {
      if (y == entry.y) { isHeader = true; break; }
    }
    if (isHeader) continue;

    std::fprintf(stderr,
                 "content bounds FAIL: %s drew \"%s\" at y=%d, above CONTENT "
                 "top %d -- the header rule slices it\n",
                 pageName, entry.text.c_str(), entry.y, Layout::CONTENT.y);
    ++g_failures;
  }
}

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

// Running off the right edge is the other half of the same defect, and the one
// that keeps coming back: "ALT+R PATTER", "SHORTER L...". Nothing catches it by
// eye reliably, and it is pure arithmetic.
void checkRightBound(const char* pageName, const RecordingGfx& gfx) {
  const int contentRight = Layout::CONTENT.x + Layout::CONTENT.w;
  for (const auto& entry : gfx.texts) {
    if (entry.y < Layout::CONTENT.y) continue;
    const int right = entry.x + gfx.textWidth(entry.text.c_str());
    if (right <= contentRight) continue;
    std::fprintf(stderr,
                 "content bounds FAIL: %s drew \"%s\" ending at x=%d, past the "
                 "right edge %d -- the tail is cut off\n",
                 pageName, entry.text.c_str(), right, contentRight);
    ++g_failures;
  }
}

// The footer stacks into two full-width rows when either hint overflows its
// half column. Both rows must land inside the 16 px band and must not share a
// pixel row with each other -- on GENRE they did both, so the top row lost its
// ascenders and the bottom row was cut by the edge of the screen.
void checkStackedFooter(RecordingGfx& gfx) {
  const int footerTop = Layout::FOOTER.y;
  const int footerBottom = Layout::FOOTER.y + Layout::FOOTER.h;

  // The real strings the Phrase editor publishes: they are the longest in use
  // and the ones that have twice been silently ellipsized.
  const char* kLeft = "SPACE LISTEN/STOP  U/D HIGHER LOWER";
  const char* kRight = "L/R PICK SOUND  ALT+L/R SHORTER LONGER";
  LayoutManager::drawFooter(gfx, kLeft, kRight);

  std::vector<RecordingGfx::TextEntry> rows;
  for (const auto& entry : gfx.texts) {
    if (entry.y >= footerTop - 4) rows.push_back(entry);
  }

  if (rows.size() != 2) {
    std::fprintf(stderr,
                 "footer FAIL: expected two stacked rows, recorded %zu\n",
                 rows.size());
    ++g_failures;
    return;
  }

  // kFont5x7GlyphHeight is 7 px of ink plus 1 px of spacing. The bottom bound
  // is about ink; the distance between rows is about the spacing the font
  // design assumes, and losing it is what made the two rows illegible.
  const int inkHeight = gfx.fontHeight() - 1;

  for (const auto& row : rows) {
    if (row.y < footerTop) {
      std::fprintf(stderr,
                   "footer FAIL: \"%s\" starts at y=%d, above the footer "
                   "band top %d\n", row.text.c_str(), row.y, footerTop);
      ++g_failures;
    }
    const int bottom = row.y + inkHeight;
    if (bottom > footerBottom) {
      std::fprintf(stderr,
                   "footer FAIL: \"%s\" bottoms out at %d, past the footer "
                   "band %d -- the screen edge cuts it\n",
                   row.text.c_str(), bottom, footerBottom);
      ++g_failures;
    }
  }

  // drawClippedText truncates and appends an ellipsis rather than overflowing,
  // so a hint that does not fit arrives on screen quietly shortened. Comparing
  // what was drawn against what was asked for is the only way to see that.
  if (rows[0].text != kLeft || rows[1].text != kRight) {
    std::fprintf(stderr,
                 "footer FAIL: a hint was truncated -- asked \"%s\" / \"%s\", "
                 "drew \"%s\" / \"%s\"\n",
                 kLeft, kRight, rows[0].text.c_str(), rows[1].text.c_str());
    ++g_failures;
  }

  if (rows[1].y - rows[0].y < gfx.fontHeight()) {
    std::fprintf(stderr,
                 "footer FAIL: rows are %d px apart, less than the font's %d -- "
                 "\"%s\" and \"%s\" touch\n",
                 rows[1].y - rows[0].y, gfx.fontHeight(),
                 rows[0].text.c_str(), rows[1].text.c_str());
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

  const std::vector<int> headerRows = headerRowsY();

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    AudioGuard guard{};
    RecordingGfx gfx;
    FeelPage page(gfx, engine, guard);
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("FeelPage", gfx);
    checkTopBound("FeelPage", gfx, headerRows);
    checkRightBound("FeelPage", gfx);
  }

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    GenrePage page(gfx, engine, AudioGuard{});
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("GenrePage", gfx);
    checkTopBound("GenrePage", gfx, headerRows);
    checkRightBound("GenrePage", gfx);
  }

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    PhrasePage page(gfx, engine, AudioGuard{}, false);
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("PhrasePage", gfx);
    checkTopBound("PhrasePage", gfx, headerRows);
    checkRightBound("PhrasePage", gfx);
  }

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    SongPage page(gfx, engine, AudioGuard{});
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("SongPage", gfx);
    checkTopBound("SongPage", gfx, headerRows);
    checkRightBound("SongPage", gfx);
  }

  {
    MiniAcid engine(kTestSampleRate, nullptr);
    MusicalEventRouter router;
    PerformanceKeyboard keyboard(router);
    RecordingGfx gfx;
    PerformPage page(gfx, engine, keyboard);
    page.onEnter(0);
    page.draw(gfx);
    checkBottomBound("PerformPage", gfx);
    checkTopBound("PerformPage", gfx, headerRows);
    checkRightBound("PerformPage", gfx);
  }

  // The Phrase editor, on the source it actually renders. It was missing from
  // this gate while being the page under the heaviest change, so two of its
  // hints were clipped on screen before anyone noticed.
  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
    page.onEnter(0);
    (void)engine.makePhrase(0);
    page.draw(gfx);
    // A gate that silently rendered the Pattern view would prove nothing, so
    // check the Phrase path was the one exercised before believing its result.
    bool drewPhrase = false;
    for (const auto& entry : gfx.texts) {
      if (entry.text == "PHRASE") drewPhrase = true;
    }
    if (!drewPhrase) {
      std::fprintf(stderr,
                   "content bounds FAIL: the Phrase editor was never rendered, "
                   "so this page was not actually under test\n");
      ++g_failures;
    }

    checkBottomBound("SynthSequencerPage(phrase)", gfx);
    checkTopBound("SynthSequencerPage(phrase)", gfx, headerRows);
    checkRightBound("SynthSequencerPage(phrase)", gfx);
  }

  // The list is the second view of the same Phrase, with its own presentation
  // and therefore its own chance to run off an edge. Leaving it out is how the
  // roll's hints got clipped twice before anyone noticed.
  {
    MiniAcid engine(kTestSampleRate, nullptr);
    RecordingGfx gfx;
    SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
    page.onEnter(0);
    (void)engine.makePhrase(0);
    UIEvent toList{};
    toList.event_type = GROOVEPUTER_KEY_DOWN;
    toList.key = 'v';
    (void)page.handleEvent(toList);
    page.draw(gfx);

    bool drewList = false;
    for (const auto& entry : gfx.texts) {
      if (entry.text == "SOUNDS") drewList = true;
    }
    if (!drewList) {
      std::fprintf(stderr,
                   "content bounds FAIL: the list view was never rendered, so "
                   "this view was not actually under test\n");
      ++g_failures;
    }

    checkBottomBound("SynthSequencerPage(list)", gfx);
    checkTopBound("SynthSequencerPage(list)", gfx, headerRows);
    checkRightBound("SynthSequencerPage(list)", gfx);
  }

  {
    RecordingGfx gfx;
    checkStackedFooter(gfx);
  }

  if (g_failures == 0) {
    std::printf("UI content bounds: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI content bounds: %d failure(s)\n", g_failures);
  return 1;
}