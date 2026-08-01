#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../ui_theme.h"
#include "src/input/performance_keyboard.h"

namespace MusicVisuals {

inline IGfxColor accentForStyle() {
    return UI::themePalette().accent;
}

inline IGfxColor secondaryForStyle() {
    return UI::themePalette().accent2;
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
    const UI::ThemePalette palette = UI::themePalette();
    const IGfxColor border = active ? activeColor : palette.dim;
    const IGfxColor fill = active ? activeColor : palette.panel;
    const IGfxColor text = active ? palette.invert : palette.text;

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

    const UI::ThemePalette palette = UI::themePalette();
    gfx.drawRect(x, y, w, h, palette.dim);
    const int innerW = w - 2;
    const int filled = static_cast<int>((static_cast<uint64_t>(current) * innerW) / total);
    if (filled > 0) gfx.fillRect(x + 1, y + 1, filled, h - 2, fillColor);
}

inline const char* pitchClassName(uint8_t note) {
    static constexpr const char* kNames[12] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };
    return kNames[note % 12];
}

inline void drawPianoKeyRow(IGfx& gfx,
                            int x,
                            int y,
                            int w,
                            int h,
                            const char* physicalKeys,
                            int keyCount,
                            const PerformanceKeyboard& keyboard) {
    if (!physicalKeys || keyCount <= 0 || w < keyCount || h < 14) return;

    constexpr int kGap = 2;
    const int keyW = (w - kGap * (keyCount - 1)) / keyCount;
    if (keyW < 12) return;

    const int usedW = keyW * keyCount + kGap * (keyCount - 1);
    const int startX = x + (w - usedW) / 2;
    const UI::ThemePalette palette = UI::themePalette();

    for (int i = 0; i < keyCount; ++i) {
        const char physical = physicalKeys[i];
        const bool held = keyboard.isPhysicalKeyHeld(physical);
        const int keyX = startX + i * (keyW + kGap);
        const IGfxColor fill = held ? palette.active : palette.text;
        const IGfxColor border = held ? palette.focus : palette.inset;
        const IGfxColor text = held ? palette.invert : palette.inset;

        gfx.fillRect(keyX, y, keyW, h, fill);
        gfx.drawRect(keyX, y, keyW, h, border);
        if (held && keyW > 6 && h > 8) {
            gfx.fillRect(keyX + 2, y + 2, keyW - 4, 3, palette.focus);
        }

        char keyLabel[2] = {
            (physical >= 'a' && physical <= 'z')
                ? static_cast<char>(physical - ('a' - 'A'))
                : physical,
            '\0'
        };
        gfx.setTextColor(text);
        gfx.drawText(keyX + (keyW - gfx.textWidth(keyLabel)) / 2,
                     y + 2,
                     keyLabel);

        uint8_t note = 0;
        if (keyboard.noteForKey(physical, note)) {
            char noteLabel[6];
            std::snprintf(noteLabel, sizeof(noteLabel), "%s%d",
                          pitchClassName(note), static_cast<int>(note / 12) - 1);
            gfx.drawText(keyX + (keyW - gfx.textWidth(noteLabel)) / 2,
                         y + h - 9,
                         noteLabel);
        }
    }
}

inline void drawPiano(IGfx& gfx,
                      int x,
                      int y,
                      int w,
                      int h,
                      const PerformanceKeyboard& keyboard) {
    if (w < 120 || h < 34) return;

    // Match the physical Cardputer keyboard: the upper QWERTY row plays one
    // octave above the lower ASDF row. Two long rows make the exact pressed key
    // visible instead of collapsing both rows into one pitch-class keyboard.
    constexpr char kUpperRow[] = "qwertyuiop";
    constexpr char kLowerRow[] = "asdfghjkl";
    constexpr int kRowGap = 3;
    const int rowH = (h - kRowGap) / 2;

    drawPianoKeyRow(gfx, x, y, w, rowH,
                    kUpperRow, static_cast<int>(sizeof(kUpperRow) - 1), keyboard);
    drawPianoKeyRow(gfx, x, y + rowH + kRowGap, w, h - rowH - kRowGap,
                    kLowerRow, static_cast<int>(sizeof(kLowerRow) - 1), keyboard);
}

inline void drawDrumPads(IGfx& gfx,
                         int x,
                         int y,
                         int w,
                         int h,
                         const PerformanceKeyboard& keyboard) {
    constexpr char kKeys[7] = {'a', 's', 'd', 'f', 'g', 'h', 'j'};
    constexpr const char* kLabels[7] = {
        "KICK", "SNR", "CLAP", "H1", "H2", "P1", "P2"
    };
    constexpr int kGap = 3;

    const int maxByWidth = (w - kGap * 6) / 7;
    if (maxByWidth < 12 || h < 12) return;

    // Seven equal square pads fit naturally across the Cardputer display and
    // avoid the old 4+3 layout where the second row had visibly wider pads.
    const int padSize = std::min(h, maxByWidth);
    const int usedW = padSize * 7 + kGap * 6;
    const int startX = x + (w - usedW) / 2;
    const int startY = y + (h - padSize) / 2;
    const UI::ThemePalette palette = UI::themePalette();

    for (int i = 0; i < 7; ++i) {
        const int padX = startX + i * (padSize + kGap);
        const bool held = keyboard.isPhysicalKeyHeld(kKeys[i]);
        const IGfxColor fill = held ? palette.active : palette.panel;
        const IGfxColor border = held ? palette.focus : palette.dim;
        const IGfxColor text = held ? palette.invert : palette.text;

        gfx.fillRect(padX, startY, padSize, padSize, fill);
        gfx.drawRect(padX, startY, padSize, padSize, border);
        if (held && padSize > 8) {
            gfx.drawRect(padX + 2, startY + 2, padSize - 4, padSize - 4,
                         palette.focus);
        }

        char keyLabel[2] = {
            static_cast<char>(kKeys[i] - ('a' - 'A')),
            '\0'
        };
        gfx.setTextColor(text);
        gfx.drawText(padX + (padSize - gfx.textWidth(keyLabel)) / 2,
                     startY + 3,
                     keyLabel);
        gfx.drawText(padX + (padSize - gfx.textWidth(kLabels[i])) / 2,
                     startY + padSize - 10,
                     kLabels[i]);
    }
}

}  // namespace MusicVisuals
