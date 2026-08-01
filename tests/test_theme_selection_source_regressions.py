#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    theme = (ROOT / "src/ui/ui_theme.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    launcher = (ROOT / "src/ui/workspace_launcher_overlay.h").read_text(encoding="utf-8")
    colors = (ROOT / "src/ui/ui_colors.h").read_text(encoding="utf-8")
    visuals = (ROOT / "src/ui/components/music_visuals.h").read_text(encoding="utf-8")

    cycle_start = theme.index("inline VisualStyle nextThemeStyle")
    cycle_end = theme.index("inline VisualStyle previousThemeStyle", cycle_start)
    cycle = theme[cycle_start:cycle_end]

    require("case VisualStyle::MINIMAL: return VisualStyle::RETRO_CLASSIC;" in cycle,
            "CARBON must switch directly to CYBER")
    require("case VisualStyle::RETRO_CLASSIC: return VisualStyle::MINIMAL;" in cycle,
            "CYBER must switch directly to CARBON")
    require("return VisualStyle::AMBER" not in cycle,
            "AMBER must not re-enter the public theme cycle")
    require("return VisualStyle::MINIMAL_DARK" not in cycle,
            "reserved STAGE must not enter the public theme cycle")

    require('#include "ui_theme.h"' in display and
            "return UI::nextThemeStyle(style);" in display,
            "global Alt+\\ theme switching must use the shared binary selector")
    require("UI::nextThemeStyle(UI::currentStyle)" in launcher,
            "launcher preview must use the same binary selector")

    require("IGfxColor(0x020406)" in theme and
            "IGfxColor(0x080D12)" in theme,
            "CARBON semantic surfaces must stay near-black")
    require("IGfxColor(0xC7D0D9)" in theme,
            "CARBON body text must be light gray rather than full white")
    require("COLOR_TEXT = IGfxColor(0xD2DAE2)" in colors,
            "legacy CARBON pages must also avoid broad pure-white text")

    require('#include "../ui_theme.h"' in visuals and
            "UI::themePalette()" in visuals,
            "PERFORM and MIDI Player visuals must consume the shared semantic palette")
    for role in ("palette.panel", "palette.text", "palette.accent2",
                 "palette.focus", "palette.active", "palette.dim",
                 "palette.invert"):
        require(role in visuals,
                f"music visuals must use semantic theme role: {role}")
    require("COLOR_ACCENT" not in visuals and "COLOR_INFO" not in visuals and
            "COLOR_WARN" not in visuals,
            "music visuals must not bypass the shared palette with legacy accent constants")

    print("theme selection + music visuals source regressions: OK")


if __name__ == "__main__":
    main()
