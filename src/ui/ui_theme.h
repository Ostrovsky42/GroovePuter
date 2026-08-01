#pragma once

#include "ui_core.h"
#include "retro_ui_theme.h"
#include "amber_ui_theme.h"

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
                IGfxColor(0x020408)
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
                IGfxColor(RetroTheme::TEXT_INVERT)
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
                IGfxColor(AmberTheme::TEXT_INVERT)
            };
        case VisualStyle::MINIMAL:
        default:
            // CARBON: the restrained, readability-first alternative to CYBER.
            // Surfaces stay near-black; body text is deliberately light gray
            // rather than white. Bright pixels are reserved for focus/activity.
            return {
                IGfxColor(0x020406), IGfxColor(0x080D12), IGfxColor(0x05090D),
                IGfxColor(0xC7D0D9), IGfxColor(0x82909C), IGfxColor(0x46515B),
                IGfxColor(0x4AC3E8), IGfxColor(0x69D89A), IGfxColor(0x77DFFF),
                IGfxColor(0x69D89A), IGfxColor(0xE6B85C), IGfxColor(0xE35D72),
                IGfxColor(0x02070A)
            };
    }
}

inline ThemePalette themePalette() {
    return themePalette(currentStyle);
}

inline const char* themeName(VisualStyle style) {
    switch (style) {
        case VisualStyle::MINIMAL: return "CARBON";
        case VisualStyle::MINIMAL_DARK: return "STAGE";
        case VisualStyle::RETRO_CLASSIC: return "CYBER";
        case VisualStyle::AMBER: return "AMBER";
    }
    return "CARBON";
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
