#pragma once

#include "display.h"

// ============================================================================
// Cassette Tape Theme System
// ============================================================================
// Two themes: WarmTape (default) and CoolTape (alternative)
// Vibe: 60% strict instrument interface + 30% cassette plastic + 10% lofi warmth

struct CassettePalette {
    IGfxColor bg;       // Background (paper/plastic)
    IGfxColor panel;    // Light case plastic
    IGfxColor ink;      // Almost black (not pure #000)
    IGfxColor accent;   // Marking color (coral/pink)
    IGfxColor led;      // Indicator/focus color (teal/amber)
    IGfxColor shadow;   // Frame shadow (gray-brown)
    IGfxColor muted;    // Muted text/inactive elements
};

// WarmTape: Dark Obsidian case + Neon Orange accent + Green LED
// High contrast, professional midnight cassette vibe
inline constexpr CassettePalette kWarmTapePalette = {
    .bg     = IGfxColor(0x0A0A0A),  // black/charcoal
    .panel  = IGfxColor(0x1F1F1F),  // dark plastic
    .ink    = IGfxColor(0xE0E0E0),  // light gray (ink is now light for visibility)
    .accent = IGfxColor(0xFF6B35),  // vivid orange
    .led    = IGfxColor(0x3CFFA0),  // neon green
    .shadow = IGfxColor(0x000000),  // pure black
    .muted  = IGfxColor(0x707070),  // medium gray
};

// CoolTape: Deep Indigo Slate + Electric Cyan + Amber LED
// Night-mode studio aesthetic
inline constexpr CassettePalette kCoolTapePalette = {
    .bg     = IGfxColor(0x0D1117),  // deep midnight blue
    .panel  = IGfxColor(0x161B22),  // slate plastic
    .ink    = IGfxColor(0xF0F6FC),  // almost white
    .accent = IGfxColor(0x58A6FF),  // electric blue
    .led    = IGfxColor(0xFF8C00),  // bright amber
    .shadow = IGfxColor(0x010409),  // deepest blue-black
    .muted  = IGfxColor(0x8B949E),  // slate gray
};

enum class CassetteTheme {
    WarmTape = 0,
    CoolTape = 1,
};

inline const CassettePalette& getPalette(CassetteTheme theme) {
    switch (theme) {
        case CassetteTheme::CoolTape:
            return kCoolTapePalette;
        case CassetteTheme::WarmTape:
        default:
            return kWarmTapePalette;
    }
}

extern CassetteTheme g_currentTheme;
