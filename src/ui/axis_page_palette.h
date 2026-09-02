#pragma once

#include "ui_colors.h"
#include "ui_core.h"

namespace AxisUI {

struct Palette {
  IGfxColor background;
  IGfxColor panel;
  IGfxColor selected;
  IGfxColor border;
  IGfxColor text;
  IGfxColor muted;
  IGfxColor genre;
  IGfxColor feel;
  IGfxColor generation;
  IGfxColor texture;
  IGfxColor warning;
};

inline Palette paletteFor(VisualStyle style) {
  switch (style) {
    case VisualStyle::RETRO_CLASSIC:
      return {
          IGfxColor(0x02040A), IGfxColor(0x0A1020), IGfxColor(0x132640),
          IGfxColor(0x35506C), IGfxColor(0xE7F6FF), IGfxColor(0x71889B),
          IGfxColor(0x00E5FF), IGfxColor(0x73E2A7), IGfxColor(0xFFD166),
          IGfxColor(0xFF4FD8), IGfxColor(0xFF8A66)};
    case VisualStyle::AMBER:
      return {
          IGfxColor(0x090603), IGfxColor(0x171006), IGfxColor(0x2A1B08),
          IGfxColor(0x6E4A18), IGfxColor(0xFFE7B0), IGfxColor(0xA77A3B),
          IGfxColor(0xFFCA58), IGfxColor(0xE6B85C), IGfxColor(0xFF9C42),
          IGfxColor(0xFFD166), IGfxColor(0xFF7B54)};
    case VisualStyle::MINIMAL_DARK:
    case VisualStyle::MINIMAL:
    default:
      return {
          COLOR_BG, COLOR_PANEL, IGfxColor(0x101A23), COLOR_LIGHT_GRAY,
          COLOR_TEXT, COLOR_MUTED, COLOR_INFO, COLOR_SYNTH_A, COLOR_WARN,
          IGfxColor(0xC978E8), COLOR_DANGER};
  }
}

inline void drawAxisTag(IGfx& gfx,
                        int x,
                        int y,
                        const char* axis,
                        const char* subtitle,
                        IGfxColor color,
                        const Palette& palette) {
  gfx.fillRect(x, y, 232, 11, palette.panel);
  gfx.drawRect(x, y, 232, 11, palette.border);
  gfx.setTextColor(color);
  gfx.drawText(x + 3, y + 2, axis);
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 58, y + 2, subtitle);
}

inline void drawValueRow(IGfx& gfx,
                         int x,
                         int y,
                         int width,
                         const char* label,
                         const char* value,
                         bool focused,
                         IGfxColor color,
                         const Palette& palette) {
  if (focused) {
    gfx.fillRect(x, y - 1, width, 11, palette.selected);
    gfx.drawRect(x, y - 1, width, 11, color);
  }
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, y + 1, label);
  gfx.setTextColor(focused ? color : palette.text);
  gfx.drawText(x + 66, y + 1, value);
}

inline void drawMeter(IGfx& gfx,
                      int x,
                      int y,
                      int width,
                      int value,
                      int maximum,
                      IGfxColor color,
                      const Palette& palette) {
  if (maximum <= 0) maximum = 1;
  if (value < 0) value = 0;
  if (value > maximum) value = maximum;
  gfx.drawRect(x, y, width, 6, palette.border);
  const int fill = (value * (width - 2)) / maximum;
  if (fill > 0) gfx.fillRect(x + 1, y + 1, fill, 4, color);
}

}  // namespace AxisUI
