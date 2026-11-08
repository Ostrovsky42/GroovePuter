// CYBER must stay reachable and must wire its own palette (option B:
// CLEAN <-> CYBER only; AMBER and STAGE are not selectable).
#include <cassert>
#include <cstring>

#include "src/ui/ui_theme.h"
#include "src/state/ui_session_state.h"

namespace UI { VisualStyle currentStyle = VisualStyle::MINIMAL; }

int main() {
    assert(UI::nextThemeStyle(VisualStyle::MINIMAL) == VisualStyle::RETRO_CLASSIC);
    assert(UI::nextThemeStyle(VisualStyle::RETRO_CLASSIC) == VisualStyle::MINIMAL);
    assert(UI::previousThemeStyle(VisualStyle::MINIMAL) == VisualStyle::RETRO_CLASSIC);
    assert(UI::previousThemeStyle(VisualStyle::RETRO_CLASSIC) == VisualStyle::MINIMAL);
    // AMBER / STAGE fall back to CLEAN and never enter the cycle.
    assert(UI::nextThemeStyle(VisualStyle::AMBER) == VisualStyle::MINIMAL);
    assert(UI::nextThemeStyle(VisualStyle::MINIMAL_DARK) == VisualStyle::MINIMAL);

    assert(std::strcmp(UI::themeName(VisualStyle::RETRO_CLASSIC), "CYBER") == 0);
    assert(std::strcmp(UI::themeName(VisualStyle::MINIMAL), "CLEAN") == 0);

    // The CYBER palette is the RetroTheme one, distinct from CLEAN.
    const UI::ThemePalette cyber = UI::themePalette(VisualStyle::RETRO_CLASSIC);
    const UI::ThemePalette clean = UI::themePalette(VisualStyle::MINIMAL);
    assert(cyber.background.color24() == RetroTheme::BG_DEEP_BLACK);
    assert(cyber.text.color24() == RetroTheme::TEXT_PRIMARY);
    assert(cyber.accent.color24() == RetroTheme::NEON_CYAN);
    assert(cyber.text.color24() != clean.text.color24());

    // Session round trip: the stored byte of CYBER is the enum value.
    assert(static_cast<uint8_t>(VisualStyle::RETRO_CLASSIC) ==
           GroovePuterState::kCyberVisualStyle);
    assert(GroovePuterState::sanitizeVisualStyle(
               static_cast<uint8_t>(VisualStyle::RETRO_CLASSIC)) ==
           static_cast<uint8_t>(VisualStyle::RETRO_CLASSIC));
    return 0;
}
