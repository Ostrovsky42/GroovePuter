#include "color_test_page.h"

#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_colors.h"
#include "../screen_geometry.h"
#include <cstdio>

ColorTestPage::ColorTestPage(IGfx& gfx, GroovePuter& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {}

bool ColorTestPage::handleEvent(UIEvent& ui_event) {
  (void)ui_event;
  return false;
}

const std::string& ColorTestPage::getTitle() const {
  return title_;
}

void ColorTestPage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, "COLOR TEST");
  LayoutManager::clearContent(gfx);

  struct Sample {
    const char* name;
    uint32_t rgb888;
    uint16_t rgb565;
  };

  static constexpr Sample kSamples[] = {
      {"R", 0xFF0000, 0xF800},
      {"G", 0x00FF00, 0x07E0},
      {"B", 0x0000FF, 0x001F},
      {"C", 0x00FFFF, 0x07FF},
      {"M", 0xFF00FF, 0xF81F},
      {"Y", 0xFFFF00, 0xFFE0},
      {"O", 0xFFA500, 0xFD20},
      {"W", 0xFFFFFF, 0xFFFF},
  };

  const int leftX = Layout::COL_1;
  const int rightX = Layout::COL_2;
  const int swatchW = 28;
  const int swatchH = 9;

  const int titleY = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(leftX, titleY, "RGB888");
  gfx.drawText(rightX, titleY, "RAW565");

  const int rowH = 11;
  const int startY = titleY + Layout::LINE_HEIGHT;

  for (size_t i = 0; i < sizeof(kSamples) / sizeof(kSamples[0]); ++i) {
    int y = startY + static_cast<int>(i) * rowH;

    // Left column: correct RGB888
    gfx.fillRect(leftX, y, swatchW, swatchH, IGfxColor(kSamples[i].rgb888));
    gfx.drawRect(leftX, y, swatchW, swatchH, COLOR_WHITE);
    char labelLeft[12];
    snprintf(labelLeft, sizeof(labelLeft), "%s %06lX", kSamples[i].name,
             static_cast<unsigned long>(kSamples[i].rgb888));
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(leftX + swatchW + 4, y + 1, labelLeft);

    // Right column: RGB565 passed raw (matches current retro theme usage)
    gfx.fillRect(rightX, y, swatchW, swatchH, IGfxColor(kSamples[i].rgb565));
    gfx.drawRect(rightX, y, swatchW, swatchH, COLOR_WHITE);
    char labelRight[10];
    snprintf(labelRight, sizeof(labelRight), "%s %04X", kSamples[i].name,
             kSamples[i].rgb565);
    gfx.drawText(rightX + swatchW + 4, y + 1, labelRight);
  }

  LayoutManager::drawFooter(gfx, "Alt+C color test", "L=RGB888  R=RAW565");
}
