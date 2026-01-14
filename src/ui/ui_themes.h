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

// WarmTape: warm cream + faded coral + soft teal
// Like a vintage TDK/Maxell cassette with bedroom lofi warmth
inline constexpr CassettePalette kWarmTapePalette = {
    .bg     = IGfxColor(0xF5F0E6),  // warm cream (paper)
    .panel  = IGfxColor(0xE8E4D8),  // light plastic
    .ink    = IGfxColor(0x1A1A1A),  // almost black
    .accent = IGfxColor(0xE07850),  // faded coral/orange
    .led    = IGfxColor(0x40C0A0),  // soft teal
    .shadow = IGfxColor(0x8A7A6A),  // gray-brown
    .muted  = IGfxColor(0x9A9080),  // muted beige
};

// CoolTape: minty gray + dusty pink + amber lamp
// More "technical" feel, like studio equipment
inline constexpr CassettePalette kCoolTapePalette = {
    .bg     = IGfxColor(0xE0EAE4),  // minty gray
    .panel  = IGfxColor(0xD0DAD4),  // cool plastic
    .ink    = IGfxColor(0x1A1A1A),  // almost black
    .accent = IGfxColor(0xC08090),  // dusty pink
    .led    = IGfxColor(0xE0A040),  // amber (lamp)
    .shadow = IGfxColor(0x6A7A70),  // gray-green
    .muted  = IGfxColor(0x808A84),  // muted mint
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
