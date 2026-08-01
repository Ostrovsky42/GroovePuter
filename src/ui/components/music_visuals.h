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

inline bool isBlackPianoPitch(uint8_t note) {
    switch (note % 12) {
        case 1:
        case 3:
        case 6:
        case 8:
        case 10:
            return true;
        default:
            return false;
    }
}

inline const uint8_t* tinyGlyph(char c) {
    static constexpr uint8_t kBlank[5] = {0, 0, 0, 0, 0};
    static constexpr uint8_t kHash[5] = {5, 7, 5, 7, 5};
    static constexpr uint8_t kA[5] = {2, 5, 7, 5, 5};
    static constexpr uint8_t kB[5] = {6, 5, 6, 5, 6};
    static constexpr uint8_t kC[5] = {3, 4, 4, 4, 3};
    static constexpr uint8_t kD[5] = {6, 5, 5, 5, 6};
    static constexpr uint8_t kE[5] = {7, 4, 6, 4, 7};
    static constexpr uint8_t kF[5] = {7, 4, 6, 4, 4};
    static constexpr uint8_t kG[5] = {3, 4, 5, 5, 3};
    static constexpr uint8_t k0[5] = {7, 5, 5, 5, 7};
    static constexpr uint8_t k1[5] = {2, 6, 2, 2, 7};
    static constexpr uint8_t k2[5] = {6, 1, 7, 4, 7};
    static constexpr uint8_t k3[5] = {6, 1, 3, 1, 6};
    static constexpr uint8_t k4[5] = {5, 5, 7, 1, 1};
    static constexpr uint8_t k5[5] = {7, 4, 6, 1, 6};
    static constexpr uint8_t k6[5] = {3, 4, 7, 5, 7};
    static constexpr uint8_t k7[5] = {7, 1, 2, 2, 2};
    static constexpr uint8_t k8[5] = {7, 5, 7, 5, 7};
    static constexpr uint8_t k9[5] = {7, 5, 7, 1, 6};

    switch (c) {
        case '#': return kHash;
        case 'A': return kA;
        case 'B': return kB;
        case 'C': return kC;
        case 'D': return kD;
        case 'E': return kE;
        case 'F': return kF;
        case 'G': return kG;
        case '0': return k0;
        case '1': return k1;
        case '2': return k2;
        case '3': return k3;
        case '4': return k4;
        case '5': return k5;
        case '6': return k6;
        case '7': return k7;
        case '8': return k8;
        case '9': return k9;
        default: return kBlank;
    }
}

inline int tinyTextWidth(const char* text) {
    if (!text || text[0] == '\0') return 0;
    return static_cast<int>(std::strlen(text)) * 4 - 1;
}

inline void drawTinyText(IGfx& gfx,
                         int x,
                         int y,
                         const char* text,
                         IGfxColor color) {
    if (!text) return;
    int cursorX = x;
    for (const char* current = text; *current != '\0'; ++current) {
        const uint8_t* rows = tinyGlyph(*current);
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((rows[row] & (1u << (2 - column))) != 0) {
                    gfx.fillRect(cursorX + column, y + row, 1, 1, color);
                }
            }
        }
        cursorX += 4;
    }
}

inline void formatNoteLabel(uint8_t note, char (&label)[5]) {
    std::snprintf(label, sizeof(label), "%s%d",
                  pitchClassName(note), static_cast<int>(note / 12) - 1);
}

inline void drawTinyNoteLabel(IGfx& gfx,
                              int centerX,
                              int y,
                              uint8_t note,
                              IGfxColor color) {
    char label[5];
    formatNoteLabel(note, label);
    drawTinyText(gfx, centerX - tinyTextWidth(label) / 2, y, label, color);
}

inline void drawPianoKeyRow(IGfx& gfx,
                            int x,
                            int y,
                            int w,
                            int h,
                            const char* physicalKeys,
                            int keyCount,
                            const PerformanceKeyboard& keyboard) {
    if (!physicalKeys || keyCount <= 0 || w < keyCount || h < 18) return;

    constexpr int kGap = 1;
    const int keyW = (w - kGap * (keyCount - 1)) / keyCount;
    if (keyW < 12) return;

    const int usedW = keyW * keyCount + kGap * (keyCount - 1);
    const int startX = x + (w - usedW) / 2;
    const UI::ThemePalette palette = UI::themePalette();

    // First draw the long white-key bed. Accidental notes are overlaid as
    // shorter black keys in a second pass, matching piano geometry while still
    // preserving one independently-highlightable physical input per slot.
    for (int i = 0; i < keyCount; ++i) {
        const char physical = physicalKeys[i];
        uint8_t note = 0;
        if (!keyboard.noteForKey(physical, note)) continue;

        const bool black = isBlackPianoPitch(note);
        const bool held = keyboard.isPhysicalKeyHeld(physical);
        const int keyX = startX + i * (keyW + kGap);
        const IGfxColor fill = (!black && held) ? palette.active : palette.text;
        const IGfxColor border = (!black && held) ? palette.focus : palette.inset;
        const IGfxColor labelColor = (!black && held) ? palette.invert : palette.inset;

        gfx.fillRect(keyX, y, keyW, h, fill);
        gfx.drawRect(keyX, y, keyW, h, border);
        if (!black && held && keyW > 6) {
            gfx.fillRect(keyX + 2, y + 2, keyW - 4, 3, palette.accent2);
        }
        if (!black) {
            drawTinyNoteLabel(gfx, keyX + keyW / 2, y + h - 7,
                              note, labelColor);
        }
    }

    const int blackH = std::max(14, (h * 2) / 3);
    const int blackW = std::max(12, keyW - 4);
    for (int i = 0; i < keyCount; ++i) {
        const char physical = physicalKeys[i];
        uint8_t note = 0;
        if (!keyboard.noteForKey(physical, note) || !isBlackPianoPitch(note)) {
            continue;
        }

        const bool held = keyboard.isPhysicalKeyHeld(physical);
        const int slotX = startX + i * (keyW + kGap);
        int blackX = slotX - blackW / 2;
        blackX = std::max(startX, std::min(blackX, startX + usedW - blackW));
        const IGfxColor fill = held ? palette.accent2 : palette.inset;
        const IGfxColor border = held ? palette.focus : palette.panel;
        const IGfxColor labelColor = held ? palette.invert : palette.text;

        gfx.fillRect(blackX, y, blackW, blackH, fill);
        gfx.drawRect(blackX, y, blackW, blackH, border);
        if (held && blackW > 6) {
            gfx.fillRect(blackX + 2, y + 2, blackW - 4, 2, palette.active);
        }
        drawTinyNoteLabel(gfx, blackX + blackW / 2, y + blackH - 7,
                          note, labelColor);
    }
}

inline void drawPiano(IGfx& gfx,
                      int x,
                      int y,
                      int w,
                      int h,
                      const PerformanceKeyboard& keyboard) {
    if (w < 120 || h < 38) return;

    // The two Cardputer note rows stay visually separate, but each is rendered
    // as a compact piano. Physical key letters are intentionally omitted; the
    // useful information is the resolved note after scale and octave mapping.
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
