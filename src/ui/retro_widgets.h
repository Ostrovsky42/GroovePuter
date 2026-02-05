#pragma once

#ifdef USE_RETRO_THEME

#include "retro_ui_theme.h"
#include "display.h"

namespace RetroWidgets {

// ═══════════════════════════════════════════════════════════
// NEON GLOW EFFECTS
// ═══════════════════════════════════════════════════════════

// Draw text with neon glow effect
inline void drawGlowText(IGfx& gfx, int x, int y, const char* text, 
                         IGfxColor glowColor, IGfxColor textColor) {
    // Glow shadow (offset slightly)
    gfx.setTextColor(glowColor);
    gfx.drawText(x-1, y, text);
    gfx.drawText(x+1, y, text);
    gfx.drawText(x, y-1, text);
    gfx.drawText(x, y+1, text);
    
    // Main text
    gfx.setTextColor(textColor);
    gfx.drawText(x, y, text);
}

// Draw glowing border
inline void drawGlowBorder(IGfx& gfx, int x, int y, int w, int h, 
                           IGfxColor color, int thickness = 1) {
    // Outer glow
    gfx.drawRect(x-1, y-1, w+2, h+2, IGfxColor(RetroTheme::FOCUS_GLOW));
    
    // Main border
    for (int i = 0; i < thickness; i++) {
        gfx.drawRect(x+i, y+i, w-i*2, h-i*2, color);
    }
}

// ═══════════════════════════════════════════════════════════
// RETRO LCD DISPLAY ELEMENTS
// ═══════════════════════════════════════════════════════════

// 7-Segment style number display (like on TR-808)
inline void draw7SegmentNumber(IGfx& gfx, int x, int y, int value, 
                                int digits, IGfxColor color) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%0*d", digits, value);
    
    gfx.fillRect(x-2, y-2, digits*8+4, 12, IGfxColor(RetroTheme::BG_INSET));
    
    gfx.setTextColor(color);
    for (int i = 0; i < digits; i++) {
        char digit[2] = {buf[i], '\0'};
        gfx.drawText(x + i*8, y, digit);
    }
}

// Classic LED indicator (like step sequencer LEDs)
inline void drawLED(IGfx& gfx, int cx, int cy, int radius, 
                    bool lit, IGfxColor color) {
    if (lit) {
        // Glow effect
        gfx.fillCircle(cx, cy, radius+1, IGfxColor(RetroTheme::FOCUS_GLOW));
        gfx.fillCircle(cx, cy, radius, color);
        // Bright center
        gfx.fillCircle(cx, cy, radius-1, IGfxColor(RetroTheme::TEXT_PRIMARY));
    } else {
        // Dim unlit state
        gfx.fillCircle(cx, cy, radius, IGfxColor(RetroTheme::BG_DARK_GRAY));
        gfx.drawCircle(cx, cy, radius, IGfxColor(RetroTheme::GRID_DIM));
    }
}

// ═══════════════════════════════════════════════════════════
// STEP SEQUENCER GRID (808/303 style)
// ═══════════════════════════════════════════════════════════

struct StepGridConfig {
    int x, y, w, h;
    int steps;           // Usually 16
    int currentStep;     // Playing position
    int cursorStep;      // Edit cursor
    bool showCursor;
    const IGfxColor* stepColors; // Array of colors per step
    const bool* stepActive;     // Array of active flags
    const bool* stepAccent;     // Array of accent flags (optional)
};

inline void drawStepGrid(IGfx& gfx, const StepGridConfig& cfg) {
    int cellW = cfg.w / cfg.steps;
    int cellH = cfg.h;
    
    for (int i = 0; i < cfg.steps; i++) {
        int cx = cfg.x + i * cellW;
        
        // Background
        IGfxColor bgColor = IGfxColor(RetroTheme::BG_PANEL);
        if (i % 4 == 0) bgColor = IGfxColor(RetroTheme::BG_DARK_GRAY); // Measure markers
        gfx.fillRect(cx, cfg.y, cellW-1, cellH-1, bgColor);
        
        // Active step
        if (cfg.stepActive && cfg.stepActive[i]) {
            IGfxColor color = cfg.stepColors ? cfg.stepColors[i] : IGfxColor(RetroTheme::NEON_CYAN);
            
            // Accent glow
            if (cfg.stepAccent && cfg.stepAccent[i]) {
                gfx.fillRect(cx+1, cfg.y+1, cellW-3, cellH-3, IGfxColor(RetroTheme::STATUS_ACCENT));
                gfx.fillRect(cx+2, cfg.y+2, cellW-5, cellH-5, color);
            } else {
                gfx.fillRect(cx+1, cfg.y+1, cellW-3, cellH-3, color);
            }
        }
        
        // Playing position indicator (moving highlight)
        if (i == cfg.currentStep) {
            drawGlowBorder(gfx, cx, cfg.y, cellW-1, cellH-1, IGfxColor(RetroTheme::STATUS_PLAYING), 2);
        }
        
        // Cursor (edit position)
        if (cfg.showCursor && i == cfg.cursorStep) {
            gfx.drawRect(cx+1, cfg.y+1, cellW-3, cellH-3, IGfxColor(RetroTheme::SELECT_BRIGHT));
        }
        
        // Grid lines
        gfx.drawRect(cx, cfg.y, cellW-1, cellH-1, IGfxColor(RetroTheme::GRID_DIM));
    }
}

// ═══════════════════════════════════════════════════════════
// TRACK DISPLAY (VU-meter style)
// ═══════════════════════════════════════════════════════════

struct TrackDisplayConfig {
    int x, y, w, h;
    const char* name;
    bool active;
    bool selected;
    bool muted;
    int level; // 0-100
    IGfxColor color;
};

inline void drawTrackDisplay(IGfx& gfx, const TrackDisplayConfig& cfg) {
    IGfxColor bgColor = cfg.selected ? IGfxColor(RetroTheme::SELECT_BG) : IGfxColor(RetroTheme::BG_PANEL);
    gfx.fillRect(cfg.x, cfg.y, cfg.w, cfg.h, bgColor);
    
    IGfxColor borderColor = cfg.selected ? IGfxColor(RetroTheme::FOCUS_BORDER) : IGfxColor(RetroTheme::GRID_MEDIUM);
    if (cfg.selected) {
        drawGlowBorder(gfx, cfg.x, cfg.y, cfg.w, cfg.h, borderColor);
    } else {
        gfx.drawRect(cfg.x, cfg.y, cfg.w, cfg.h, borderColor);
    }
    
    int nameY = cfg.y + 2;
    IGfxColor nameColor = cfg.muted ? IGfxColor(RetroTheme::TEXT_DIM) : IGfxColor(RetroTheme::TEXT_PRIMARY);
    if (cfg.selected) {
        drawGlowText(gfx, cfg.x + 4, nameY, cfg.name, IGfxColor(RetroTheme::FOCUS_GLOW), nameColor);
    } else {
        gfx.setTextColor(nameColor);
        gfx.drawText(cfg.x + 4, nameY, cfg.name);
    }
    
    int ledX = cfg.x + cfg.w - 8;
    int ledY = cfg.y + cfg.h/2;
    drawLED(gfx, ledX, ledY, 2, cfg.active && !cfg.muted, cfg.color);
    
    if (cfg.level > 0 && !cfg.muted) {
        int meterW = (cfg.w - 40) * cfg.level / 100;
        int meterX = cfg.x + 30;
        int meterY = cfg.y + cfg.h - 4;
        int meterH = 2;
        gfx.fillRect(meterX, meterY, meterW, meterH, cfg.color);
    }
}

// ═══════════════════════════════════════════════════════════
// PATTERN/BANK SELECTOR (Classic hardware style)
// ═══════════════════════════════════════════════════════════

struct SelectorConfig {
    int x, y, w, h;
    const char* label;
    int count;       // Number of slots
    int selected;    // Currently selected
    int cursor;      // Cursor position
    bool showCursor;
    bool enabled;
};

inline void drawSelector(IGfx& gfx, const SelectorConfig& cfg) {
    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
    gfx.drawText(cfg.x, cfg.y, cfg.label);
    
    int labelW = strlen(cfg.label) * 6 + 4;
    int slotX = cfg.x + labelW;
    int slotW = (cfg.w - labelW) / cfg.count;
    
    for (int i = 0; i < cfg.count; i++) {
        int sx = slotX + i * slotW;
        
        IGfxColor bgColor = IGfxColor(RetroTheme::BG_INSET);
        if (i == cfg.selected) bgColor = IGfxColor(RetroTheme::NEON_CYAN);
        
        gfx.fillRect(sx, cfg.y, slotW-2, cfg.h, bgColor);
        
        if (cfg.showCursor && i == cfg.cursor) {
            gfx.drawRect(sx-1, cfg.y-1, slotW, cfg.h+2, IGfxColor(RetroTheme::SELECT_BRIGHT));
        }
        
        char slotChar = (i < 8) ? ('1' + i) : ('A' + (i - 8));
        char slotStr[2] = {slotChar, '\0'};
        IGfxColor textColor = (i == cfg.selected) ? IGfxColor(RetroTheme::BG_DEEP_BLACK) : IGfxColor(RetroTheme::TEXT_SECONDARY);
        gfx.setTextColor(textColor);
        gfx.drawText(sx + slotW/2 - 3, cfg.y + 1, slotStr);
    }
}

// ═══════════════════════════════════════════════════════════
// HEADER BAR (Classic synth top panel)
// ═══════════════════════════════════════════════════════════

inline void drawHeaderBar(IGfx& gfx, int x, int y, int w, int h,
                          const char* title, const char* mode,
                          bool playing, int bpm, int step) {
    (void)step;
    gfx.fillRect(x, y, w, h, IGfxColor(RetroTheme::BG_DARK_GRAY));
    gfx.drawLine(x, y+h-1, x+w, y+h-1, IGfxColor(RetroTheme::GRID_MEDIUM));
    
    drawGlowText(gfx, x + 4, y + 2, title, IGfxColor(RetroTheme::FOCUS_GLOW), IGfxColor(RetroTheme::NEON_CYAN));
    
    int modeX = x + 80;
    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
    gfx.drawText(modeX, y + 2, "MODE:");
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_ORANGE));
    gfx.drawText(modeX + 32, y + 2, mode);
    
    int statusX = x + w - 80;
    drawLED(gfx, statusX, y + h/2, 3, playing, IGfxColor(RetroTheme::STATUS_PLAYING));
    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
    gfx.drawText(statusX + 8, y + 2, playing ? "PLAY" : "STOP");
    
    draw7SegmentNumber(gfx, x + w - 50, y + 2, bpm, 3, IGfxColor(RetroTheme::NEON_YELLOW));
    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_DIM));
    gfx.drawText(x + w - 24, y + 2, "BPM");
}

// ═══════════════════════════════════════════════════════════
// FOOTER BAR (Key hints, classic style)
// ═══════════════════════════════════════════════════════════

inline void drawFooterBar(IGfx& gfx, int x, int y, int w, int h,
                          const char* leftHints, const char* rightHints,
                          const char* focusMode = nullptr) {
    gfx.fillRect(x, y, w, h, IGfxColor(RetroTheme::BG_DARK_GRAY));
    gfx.drawLine(x, y, x+w, y, IGfxColor(RetroTheme::GRID_MEDIUM));
    
    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
    gfx.drawText(x + 2, y + 2, leftHints);
    
    int rightW = strlen(rightHints) * 6;
    gfx.drawText(x + w - rightW - 2, y + 2, rightHints);
    
    if (focusMode) {
        int centerX = x + w/2;
        gfx.setTextColor(IGfxColor(RetroTheme::NEON_ORANGE));
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
    for (int sy = y; sy < y + h; sy += RetroTheme::SCANLINE_SPACING) {
        gfx.drawLine(x, sy, x + w, sy, IGfxColor(RetroTheme::SCANLINE_COLOR));
    }
}

} // namespace RetroWidgets

#endif // USE_RETRO_THEME
