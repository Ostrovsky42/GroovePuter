#pragma once

#include "ui_core.h"
#include "retro_ui_theme.h"
#include "amber_ui_theme.h"
#include "genre_palette.h"

namespace UI {

// Global style state is owned by ui_common.cpp. The palette API lives in this
// small header so common widgets can be themed without depending on ui_common.h.
extern VisualStyle currentStyle;

struct ThemePalette {
    IGfxColor background;
    IGfxColor panel;
    IGfxColor inset;
    IGfxColor text;
    IGfxColor secondary;
    IGfxColor dim;
    IGfxColor accent;
    IGfxColor accent2;
    IGfxColor focus;
    IGfxColor active;
    IGfxColor warning;
    IGfxColor danger;
    IGfxColor invert;
    // Grid lines and empty-cell outlines.
    IGfxColor grid;
    // Voice identity: CYBER uses hue; CLEAN uses text (identity by label and position).
    IGfxColor voiceA;
    IGfxColor voiceB;
    // Shared widget surfaces (CYBER widgets read these; CLEAN maps them to plain black/rules).
    IGfxColor glow;
    IGfxColor gridDim;
    IGfxColor fillAlt;
    IGfxColor selectBg;
    IGfxColor selectBright;
    // Role tokens: what a colour means on screen, not which hue it is.
    // CYBER maps them to its hues; CLEAN maps them to text, secondary or rule.
    IGfxColor title;
    IGfxColor value;
    IGfxColor modeTag;
    IGfxColor selection;
    IGfxColor noteAccent;

    IGfxColor voice(int voiceIndex) const { return voiceIndex == 0 ? voiceA : voiceB; }
};

inline ThemePalette themePalette(VisualStyle style) {
    switch (style) {
        case VisualStyle::MINIMAL_DARK:
            // Reserved camera/stage variant. Kept out of the public cycle until
            // every specialized page has an explicit MINIMAL_DARK treatment.
            return {
                IGfxColor(0x010204), IGfxColor(0x080B10), IGfxColor(0x04070B),
                IGfxColor(0xF7FAFC), IGfxColor(0xA7B0BB), IGfxColor(0x58616B),
                IGfxColor(0xC7FF3D), IGfxColor(0x56D4FF), IGfxColor(0xC7FF3D),
                IGfxColor(0x8BFFB0), IGfxColor(0xFFD166), IGfxColor(0xFF4D6D),
                IGfxColor(0x020408),
                IGfxColor(0x1A2230), IGfxColor(0x56D4FF), IGfxColor(0xC7FF3D),
                IGfxColor(0x0B1018), IGfxColor(0x0B1018), IGfxColor(0x0B1018), IGfxColor(0x132030), IGfxColor(0xE0E8F0),
                IGfxColor(0x56D4FF), IGfxColor(0xC7FF3D), IGfxColor(0xA7B0BB), IGfxColor(0x56D4FF), IGfxColor(0xC7FF3D)
            };
        case VisualStyle::RETRO_CLASSIC:
            return {
                IGfxColor(RetroTheme::BG_DEEP_BLACK),
                IGfxColor(RetroTheme::BG_PANEL),
                IGfxColor(RetroTheme::BG_INSET),
                IGfxColor(RetroTheme::TEXT_PRIMARY),
                IGfxColor(RetroTheme::TEXT_SECONDARY),
                IGfxColor(RetroTheme::TEXT_DIM),
                IGfxColor(RetroTheme::NEON_CYAN),
                IGfxColor(RetroTheme::NEON_MAGENTA),
                IGfxColor(RetroTheme::FOCUS_BORDER),
                IGfxColor(RetroTheme::STATUS_ACTIVE),
                IGfxColor(RetroTheme::NEON_YELLOW),
                IGfxColor(RetroTheme::STATUS_ACCENT),
                IGfxColor(RetroTheme::TEXT_INVERT),
                IGfxColor(RetroTheme::GRID_MEDIUM), IGfxColor(RetroTheme::NEON_CYAN), IGfxColor(RetroTheme::NEON_MAGENTA),
                IGfxColor(RetroTheme::FOCUS_GLOW), IGfxColor(RetroTheme::GRID_DIM), IGfxColor(RetroTheme::BG_DARK_GRAY),
                IGfxColor(RetroTheme::SELECT_BG), IGfxColor(RetroTheme::SELECT_BRIGHT),
                IGfxColor(RetroTheme::NEON_CYAN), IGfxColor(RetroTheme::NEON_YELLOW), IGfxColor(RetroTheme::NEON_ORANGE),
                IGfxColor(RetroTheme::NEON_CYAN), IGfxColor(RetroTheme::NEON_ORANGE)
            };
        case VisualStyle::AMBER:
            // Legacy enum value retained for compatibility with existing page
            // switches. It is no longer part of the public theme cycle.
            return {
                IGfxColor(AmberTheme::BG_DEEP_BLACK),
                IGfxColor(AmberTheme::BG_PANEL),
                IGfxColor(AmberTheme::BG_INSET),
                IGfxColor(AmberTheme::TEXT_PRIMARY),
                IGfxColor(AmberTheme::TEXT_SECONDARY),
                IGfxColor(AmberTheme::TEXT_DIM),
                IGfxColor(AmberTheme::NEON_CYAN),
                IGfxColor(AmberTheme::NEON_MAGENTA),
                IGfxColor(AmberTheme::FOCUS_BORDER),
                IGfxColor(AmberTheme::STATUS_ACTIVE),
                IGfxColor(AmberTheme::NEON_YELLOW),
                IGfxColor(AmberTheme::NEON_ORANGE),
                IGfxColor(AmberTheme::TEXT_INVERT),
                IGfxColor(AmberTheme::GRID_MEDIUM), IGfxColor(AmberTheme::NEON_CYAN), IGfxColor(AmberTheme::NEON_MAGENTA),
                IGfxColor(AmberTheme::FOCUS_GLOW), IGfxColor(AmberTheme::GRID_DIM), IGfxColor(AmberTheme::BG_DARK_GRAY),
                IGfxColor(AmberTheme::SELECT_BG), IGfxColor(AmberTheme::SELECT_BRIGHT),
                IGfxColor(AmberTheme::NEON_CYAN), IGfxColor(AmberTheme::NEON_YELLOW), IGfxColor(AmberTheme::NEON_ORANGE),
                IGfxColor(AmberTheme::NEON_CYAN), IGfxColor(AmberTheme::NEON_ORANGE)
            };
        case VisualStyle::MINIMAL:
        default:
            // CLEAN (formerly CARBON), Warm palette (docs/design/CLEAN_THEME_SPEC.md).
            // Monochrome on true black: warm white text, warm grey secondary and dim.
            // One accent hue (orange) marks live state only; it is never the only cue.
            // No separate panel colour: surfaces are separated by rules and spacing.
            // Values are chosen on the RGB565 grid so warm greys do not drift.
            return {
                IGfxColor(0x000000), IGfxColor(0x000000), IGfxColor(0x000000),
                IGfxColor(0xE7E3DE), IGfxColor(0x9C9A94), IGfxColor(0x6B6963),
                IGfxColor(0xFF7931), IGfxColor(0xFF7931), IGfxColor(0xE7E3DE),
                IGfxColor(0xFF7931), IGfxColor(0xFF7931), IGfxColor(0xFF7931),
                IGfxColor(0x000000),
                IGfxColor(0x6B6963), IGfxColor(0xE7E3DE), IGfxColor(0xE7E3DE),
                IGfxColor(0x000000), IGfxColor(0x393734), IGfxColor(0x000000), IGfxColor(0x1C1B19), IGfxColor(0xE7E3DE),
                IGfxColor(0xE7E3DE), IGfxColor(0xE7E3DE), IGfxColor(0x9C9A94), IGfxColor(0xE7E3DE), IGfxColor(0xE7E3DE)
            };
    }
}

inline ThemePalette themePalette() {
    return themePalette(currentStyle);
}

inline const char* themeName(VisualStyle style) {
    switch (style) {
        case VisualStyle::MINIMAL: return "CLEAN";
        case VisualStyle::MINIMAL_DARK: return "STAGE";
        case VisualStyle::RETRO_CLASSIC: return "CYBER";
        case VisualStyle::AMBER: return "AMBER";
    }
    return "CLEAN";
}

// Public selection is intentionally binary after hardware review:
// CYBER is the expressive/default instrument identity; CARBON is the darker,
// calmer readability-first alternative. AMBER is retained only for legacy
// page compatibility. MINIMAL_DARK/STAGE remains a reserved future slot.
inline VisualStyle nextThemeStyle(VisualStyle style) {
    switch (style) {
        case VisualStyle::MINIMAL: return VisualStyle::RETRO_CLASSIC;
        case VisualStyle::RETRO_CLASSIC: return VisualStyle::MINIMAL;
        case VisualStyle::AMBER:
        case VisualStyle::MINIMAL_DARK:
        default:
            return VisualStyle::MINIMAL;
    }
}

inline VisualStyle previousThemeStyle(VisualStyle style) {
    return nextThemeStyle(style);
}

} // namespace UI
