#pragma once

#ifdef USE_AMBER_THEME

#include "amber_ui_theme.h"
#include "display.h"
#include <cstdio>
#include <cstring>

namespace AmberWidgets {

// ═══════════════════════════════════════════════════════════
// NEON GLOW EFFECTS
// ═══════════════════════════════════════════════════════════

inline void drawGlowText(IGfx& gfx, int x, int y, const char* text, 
                         IGfxColor glowColor, IGfxColor textColor) {
    gfx.setTextColor(glowColor);
    gfx.drawText(x-1, y, text);
    gfx.drawText(x+1, y, text);
    gfx.drawText(x, y-1, text);
    gfx.drawText(x, y+1, text);
    
    gfx.setTextColor(textColor);
    gfx.drawText(x, y, text);
}

inline void drawGlowBorder(IGfx& gfx, int x, int y, int w, int h, 
                           IGfxColor color, int thickness = 1) {
    gfx.drawRect(x-1, y-1, w+2, h+2, IGfxColor(AmberTheme::FOCUS_GLOW));
    for (int i = 0; i < thickness; i++) {
        gfx.drawRect(x+i, y+i, w-i*2, h-i*2, color);
    }
}

// ═══════════════════════════════════════════════════════════
// RETRO LCD DISPLAY ELEMENTS
// ═══════════════════════════════════════════════════════════

inline void draw7SegmentNumber(IGfx& gfx, int x, int y, int value, 
                                int digits, IGfxColor color) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%0*d", digits, value);
    
    gfx.fillRect(x-2, y-2, digits*8+4, 12, IGfxColor(AmberTheme::BG_INSET));
    
    gfx.setTextColor(color);
    for (int i = 0; i < digits; i++) {
        char digit[2] = {buf[i], '\0'};
        gfx.drawText(x + i*8, y, digit);
    }
}

inline void drawLED(IGfx& gfx, int cx, int cy, int radius, 
                    bool lit, IGfxColor color) {
    if (lit) {
        gfx.fillCircle(cx, cy, radius+1, IGfxColor(AmberTheme::FOCUS_GLOW));
        gfx.fillCircle(cx, cy, radius, color);
        gfx.fillCircle(cx, cy, radius-1, IGfxColor(AmberTheme::TEXT_PRIMARY));
    } else {
        gfx.fillCircle(cx, cy, radius, IGfxColor(AmberTheme::BG_DARK_GRAY));
        gfx.drawCircle(cx, cy, radius, IGfxColor(AmberTheme::GRID_DIM));
    }
}

// ═══════════════════════════════════════════════════════════
// STEP SEQUENCER GRID (808/303 style)
// ═══════════════════════════════════════════════════════════

struct StepGridConfig {
    int x, y, w, h;
    int steps;
    int currentStep;
    int cursorStep;
    bool showCursor;
    const IGfxColor* stepColors;
    const bool* stepActive;
    const bool* stepAccent;
};

inline void drawStepGrid(IGfx& gfx, const StepGridConfig& cfg) {
    int cellW = cfg.w / cfg.steps;
    int cellH = cfg.h;
    
    for (int i = 0; i < cfg.steps; i++) {
        int cx = cfg.x + i * cellW;
        
        IGfxColor bgColor = IGfxColor(AmberTheme::BG_PANEL);
        if (i % 4 == 0) bgColor = IGfxColor(AmberTheme::BG_DARK_GRAY);
        gfx.fillRect(cx, cfg.y, cellW-1, cellH-1, bgColor);
        
        if (cfg.stepActive && cfg.stepActive[i]) {
            IGfxColor color = cfg.stepColors ? cfg.stepColors[i] : IGfxColor(AmberTheme::NEON_CYAN);
            if (cfg.stepAccent && cfg.stepAccent[i]) {
                gfx.fillRect(cx+1, cfg.y+1, cellW-3, cellH-3, IGfxColor(AmberTheme::STATUS_ACCENT));
                gfx.fillRect(cx+2, cfg.y+2, cellW-5, cellH-5, color);
            } else {
                gfx.fillRect(cx+1, cfg.y+1, cellW-3, cellH-3, color);
            }
        }
        
        if (i == cfg.currentStep) {
            drawGlowBorder(gfx, cx, cfg.y, cellW-1, cellH-1, IGfxColor(AmberTheme::STATUS_PLAYING), 2);
        }
        
        if (cfg.showCursor && i == cfg.cursorStep) {
            drawGlowBorder(gfx, cx, cfg.y, cellW-1, cellH-1, IGfxColor(AmberTheme::SELECT_BRIGHT), 1);
        }
    }
}

// ═══════════════════════════════════════════════════════════
// NAVIGATION BAR (Bank/Pattern selection)
// ═══════════════════════════════════════════════════════════

struct SelectionBarConfig {
    int x, y, w, h;
    int count;
    int selected;
    int cursor;
    bool showCursor;
    const char* label;
};

inline void drawSelectionBar(IGfx& gfx, const SelectionBarConfig& cfg) {
    int labelW = strlen(cfg.label) * 6 + 4;
    gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
    gfx.drawText(cfg.x, cfg.y + 1, cfg.label);
    
    int slotX = cfg.x + labelW;
    int slotW = (cfg.w - labelW) / cfg.count;
    
    for (int i = 0; i < cfg.count; i++) {
        int sx = slotX + i * slotW;
        
        IGfxColor bgColor = IGfxColor(AmberTheme::BG_INSET);
        if (i == cfg.selected) bgColor = IGfxColor(AmberTheme::NEON_CYAN);
        
        gfx.fillRect(sx, cfg.y, slotW-2, cfg.h, bgColor);
        
        if (cfg.showCursor && i == cfg.cursor) {
            gfx.drawRect(sx-1, cfg.y-1, slotW, cfg.h+2, IGfxColor(AmberTheme::SELECT_BRIGHT));
        }
        
        char slotChar = (i < 8) ? ('1' + i) : ('A' + (i - 8));
        char slotStr[2] = {slotChar, '\0'};
        IGfxColor textColor = (i == cfg.selected) ? IGfxColor(AmberTheme::BG_DEEP_BLACK) : IGfxColor(AmberTheme::TEXT_SECONDARY);
        gfx.setTextColor(textColor);
        gfx.drawText(sx + slotW/2 - 3, cfg.y + 1, slotStr);
    }
}

// ═══════════════════════════════════════════════════════════
// HEADER BAR
// ═══════════════════════════════════════════════════════════

inline void drawHeaderBar(IGfx& gfx, int x, int y, int w, int h,
                          const char* title, const char* mode,
                          bool playing, int bpm, int step) {
    (void)step;
    gfx.fillRect(x, y, w, h, IGfxColor(AmberTheme::BG_DARK_GRAY));
    gfx.drawLine(x, y+h-1, x+w, y+h-1, IGfxColor(AmberTheme::GRID_MEDIUM));
    
    drawGlowText(gfx, x + 4, y + 2, title, IGfxColor(AmberTheme::FOCUS_GLOW), IGfxColor(AmberTheme::NEON_CYAN));
    
    int modeX = x + 45; // Moved left
    gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
    gfx.drawText(modeX, y + 2, "MODE:");
    gfx.setTextColor(IGfxColor(AmberTheme::NEON_ORANGE));
    gfx.drawText(modeX + 32, y + 2, mode);
    
    int statusX = x + w - 80;
    drawLED(gfx, statusX, y + h/2, 3, playing, IGfxColor(AmberTheme::STATUS_PLAYING));
    gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
    gfx.drawText(statusX + 8, y + 2, playing ? "PLAY" : "STOP");
    
    draw7SegmentNumber(gfx, x + w - 50, y + 2, bpm, 3, IGfxColor(AmberTheme::NEON_YELLOW));
    gfx.setTextColor(IGfxColor(AmberTheme::TEXT_DIM));
    gfx.drawText(x + w - 24, y + 2, "BPM");
}

// ═══════════════════════════════════════════════════════════
// FOOTER BAR
// ═══════════════════════════════════════════════════════════

inline void drawFooterBar(IGfx& gfx, int x, int y, int w, int h,
                          const char* leftHints, const char* rightHints,
                          const char* focusMode = nullptr) {
    gfx.fillRect(x, y, w, h, IGfxColor(AmberTheme::BG_DARK_GRAY));
    gfx.drawLine(x, y, x+w, y, IGfxColor(AmberTheme::GRID_MEDIUM));
    
    gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
    gfx.drawText(x + 2, y + 2, leftHints);
    
    int rightW = strlen(rightHints) * 6;
    gfx.drawText(x + w - rightW - 2, y + 2, rightHints);
    
    if (focusMode) {
        int centerX = x + w/2;
        gfx.setTextColor(IGfxColor(AmberTheme::NEON_ORANGE));
        char buf[32];
        snprintf(buf, sizeof(buf), "[%s]", focusMode);
        int focusW = strlen(buf) * 6;
        gfx.drawText(centerX - focusW/2, y + 2, buf);
    }
}

// ═══════════════════════════════════════════════════════════
// SCANLINE OVERLAY (CRT effect)
// ═══════════════════════════════════════════════════════════

inline void drawScanlines(IGfx& gfx, int x, int y, int w, int h) {
    for (int sy = y; sy < y + h; sy += AmberTheme::SCANLINE_SPACING) {
        gfx.drawLine(x, sy, x + w, sy, IGfxColor(AmberTheme::SCANLINE_COLOR));
    }
}

} // namespace AmberWidgets

#endif // USE_AMBER_THEME
