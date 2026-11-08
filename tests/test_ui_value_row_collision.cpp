// Slice 1.1: a value must never be drawn on top of its own label.
//
// AxisUI::drawValueRow places the label at x+2 and the value at a hard-coded
// x+66. Several labels in production use are wider than 64 px, so the value
// lands inside the label's glyphs. On the FEEL screen this produces three
// collisions at once, visible as "SWING OFFBE50%" and "VELOCITY VA50%"
// (screens/ui-pass-2026-09-07/feel-collisions.png).
//
// This is a behavioural test, not a source grep: it drives the real widget
// through a recording IGfx and reads back the coordinates it asked for. The
// font metric is the honest one for this display -- a fixed 6 px advance --
// because the defect only exists relative to real glyph widths.
//
// The fix must not simply right-align every value either: rows with short
// labels have to stay on a shared column, otherwise a list of settings turns
// into a ragged staircase. Both properties are asserted below.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/ui/axis_page_palette.h"

namespace {

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
    texts.push_back({x, y, text ? text : ""});
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

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "value row FAIL: %s\n", message);
  ++g_failures;
}

struct Row {
  int labelX;
  int labelEndX;
  int valueX;
};

// Draw one row and report where the widget put the two strings.
Row measure(const char* label, const char* value, int x, int width) {
  RecordingGfx gfx;
  const AxisUI::Palette palette = AxisUI::paletteFor(VisualStyle::RETRO_CLASSIC);
  AxisUI::drawValueRow(gfx, x, 20, width, label, value, false,
                       palette.feel, palette);

  Row row{-1, -1, -1};
  for (const auto& entry : gfx.texts) {
    if (entry.text == label) {
      row.labelX = entry.x;
      row.labelEndX = entry.x + gfx.textWidth(label);
    } else if (entry.text == value) {
      row.valueX = entry.x;
    }
  }
  return row;
}

// Every label the FEEL screen actually passes to this widget
// (src/ui/pages/feel_page.cpp). "SWING OFFBEAT" is the widest at 78 px.
const char* const kFeelLabels[] = {
    "PROFILE", "SWING OFFBEAT", "FEEL AMOUNT", "VELOCITY VAR",
    "FEEL CYCLE", "PRESET"};

}  // namespace

int main() {
  constexpr int kX = 4;
  constexpr int kWidth = 232;

  // 1. No label is ever overlapped by its own value.
  for (const char* label : kFeelLabels) {
    const Row row = measure(label, "50%", kX, kWidth);
    expect(row.labelX >= 0 && row.valueX >= 0, "row did not draw both strings");
    char message[96];
    std::snprintf(message, sizeof(message),
                  "value overlaps label \"%s\"", label);
    expect(row.valueX >= row.labelEndX, message);
  }

  // 2. Short labels keep a shared column, so a settings list stays a column
  //    and not a staircase. PROFILE, PRESET and FEEL CYCLE all fit inside the
  //    established 66 px column and must land on exactly the same x.
  const Row profile = measure("PROFILE", "STRAIGHT", kX, kWidth);
  const Row preset = measure("PRESET", "HUMAN", kX, kWidth);
  const Row cycle = measure("FEEL CYCLE", "1 BAR", kX, kWidth);
  expect(profile.valueX == preset.valueX,
         "short-label rows lost their shared value column");
  expect(cycle.valueX == preset.valueX,
         "FEEL CYCLE left the shared value column although it fits");

  // 3. A value is never pushed off the right edge of its row.
  {
    RecordingGfx gfx;
    const Row wide = measure("SWING OFFBEAT", "100%", kX, kWidth);
    expect(wide.valueX + gfx.textWidth("100%") <= kX + kWidth,
           "value runs past the right edge of the row");
  }

  // 4. The widest label still leaves room: this is the case that fails today.
  {
    RecordingGfx gfx;
    const Row wide = measure("SWING OFFBEAT", "50%", kX, kWidth);
    expect(wide.valueX >= kX + 2 + gfx.textWidth("SWING OFFBEAT"),
           "SWING OFFBEAT is still drawn under its value");
  }

  if (g_failures == 0) {
    std::printf("UI value row collision: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI value row collision: %d failure(s)\n", g_failures);
  return 1;
}
