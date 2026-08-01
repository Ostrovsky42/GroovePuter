#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "../ui_colors.h"
#include "../ui_common.h"
#include "src/input/performance_keyboard.h"

namespace MusicVisuals {

inline IGfxColor accentForStyle() {
    switch (UI::currentStyle) {
        case VisualStyle::RETRO_CLASSIC: return COLOR_INFO;
        case VisualStyle::AMBER: return COLOR_WARN;
        case VisualStyle::MINIMAL_DARK: return COLOR_ACCENT;
        case VisualStyle::MINIMAL:
        default: return COLOR_ACCENT;
    }
}

inline IGfxColor secondaryForStyle() {
    switch (UI::currentStyle) {
        case VisualStyle::RETRO_CLASSIC: return COLOR_WARN;
        case VisualStyle::AMBER: return COLOR_INFO;
        case VisualStyle::MINIMAL_DARK: return COLOR_INFO;
        case VisualStyle::MINIMAL:
        default: return COLOR_INFO;
    }
}

inline int chipWidth(const char* label) {
    if (!label) return 10;
    return static_cast<int>(std::strlen(label)) * 6 + 8;
}

inline int drawChip(IGfx& gfx,
                    int x,
                    int y,
                    const char* label,
                    bool active = false,
                    IGfxColor activeColor = accentForStyle()) {
    const int w = chipWidth(label);
    const int h = 10;
    const IGfxColor border = active ? activeColor : COLOR_LIGHT_GRAY;
    const IGfxColor fill = active ? activeColor : COLOR_PANEL;
    const IGfxColor text = active ? COLOR_BG : COLOR_TEXT;

    gfx.fillRect(x, y, w, h, fill);
    gfx.drawRect(x, y, w, h, border);
    gfx.setTextColor(text);
    gfx.drawText(x + 4, y + 1, label ? label : "");
    return w;
}

inline void drawProgressBar(IGfx& gfx,
                            int x,
                            int y,
                            int w,
                            int h,
                            uint32_t current,
                            uint32_t total,
                            IGfxColor fillColor = accentForStyle()) {
    if (w < 4 || h < 4) return;
    if (total == 0) total = 1;
    if (current > total) current = total;

    gfx.drawRect(x, y, w, h, COLOR_LIGHT_GRAY);
    const int innerW = w - 2;
    const int filled = static_cast<int>((static_cast<uint64_t>(current) * innerW) / total);
    if (filled > 0) gfx.fillRect(x + 1, y + 1, filled, h - 2, fillColor);
}

inline void drawPiano(IGfx& gfx,
                      int x,
                      int y,
                      int w,
                      int h,
                      const PerformanceKeyboard& keyboard) {
    if (w < 70 || h < 24) return;

    constexpr uint8_t kWhitePitchClasses[7] = {0, 2, 4, 5, 7, 9, 11};
    constexpr uint8_t kBlackPitchClasses[5] = {1, 3, 6, 8, 10};
    constexpr uint8_t kBlackAfterWhite[5] = {0, 1, 3, 4, 5};
    constexpr const char* kWhiteLabels[7] = {"C", "D", "E", "F", "G", "A", "B"};

    const int whiteW = std::max(1, w / 7);
    const int blackW = std::max(5, whiteW / 2);
    const int blackH = std::max(12, (h * 3) / 5);
    const IGfxColor accent = accentForStyle();

    for (int i = 0; i < 7; ++i) {
        const int wx = x + i * whiteW;
        const int ww = (i == 6) ? (x + w - wx) : whiteW;
        const bool held = keyboard.isPitchClassHeld(kWhitePitchClasses[i]);
        gfx.fillRect(wx, y, ww, h, held ? accent : COLOR_TEXT);
        gfx.drawRect(wx, y, ww, h, COLOR_DARKER);
        gfx.setTextColor(held ? COLOR_BG : COLOR_DARKER);
        gfx.drawText(wx + 2, y + h - 9, kWhiteLabels[i]);
    }

    for (int i = 0; i < 5; ++i) {
        const int boundary = x + (static_cast<int>(kBlackAfterWhite[i]) + 1) * whiteW;
        const int bx = boundary - blackW / 2;
        const bool held = keyboard.isPitchClassHeld(kBlackPitchClasses[i]);
        gfx.fillRect(bx, y, blackW, blackH, held ? secondaryForStyle() : COLOR_DARKER);
        gfx.drawRect(bx, y, blackW, blackH, held ? COLOR_TEXT : COLOR_PANEL);
    }
}

inline void drawDrumPads(IGfx& gfx,
                         int x,
                         int y,
                         int w,
                         int h,
                         const PerformanceKeyboard& keyboard) {
    constexpr char kKeys[7] = {'a', 's', 'd', 'f', 'g', 'h', 'j'};
    constexpr const char* kLabels[7] = {
        "A KICK", "S SNARE", "D CLAP", "F HAT1",
        "G HAT2", "H PERC1", "J PERC2"
    };

    const int gap = 3;
    const int rowH = std::max(12, (h - gap) / 2);
    const int topW = std::max(20, (w - gap * 3) / 4);
    const int bottomW = std::max(20, (w - gap * 2) / 3);
    const IGfxColor accent = accentForStyle();

    for (int i = 0; i < 7; ++i) {
        const bool bottom = i >= 4;
        const int column = bottom ? i - 4 : i;
        const int padW = bottom ? bottomW : topW;
        const int px = x + column * (padW + gap);
        const int py = y + (bottom ? rowH + gap : 0);
        const bool held = keyboard.isPhysicalKeyHeld(kKeys[i]);

        gfx.fillRect(px, py, padW, rowH, held ? accent : COLOR_PANEL);
        gfx.drawRect(px, py, padW, rowH, held ? COLOR_TEXT : COLOR_LIGHT_GRAY);
        gfx.setTextColor(held ? COLOR_BG : COLOR_TEXT);
        gfx.drawText(px + 3, py + 3, kLabels[i]);
    }
}

}  // namespace MusicVisuals
