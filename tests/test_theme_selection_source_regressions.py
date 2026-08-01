#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_theme_selection() -> None:
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


def test_workflow_local_page_navigation() -> None:
    workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    launcher = (ROOT / "src/ui/workspace_launcher_overlay.h").read_text(encoding="utf-8")
    settings = (ROOT / "src/ui/pages/settings_page.cpp").read_text(encoding="utf-8")

    for mode in ("Perform", "Generate", "Hub", "Song", "Settings"):
        require(f"WorkflowMode::{mode}" in workflow,
                f"missing top-level workflow: {mode}")
    require("constexpr int count = 5;" in workflow,
            "Fn+Tab must remain a bounded five-workflow ring")

    require("kPerform, kPlayer" in workflow,
            "PERFORM workflow must contain keyboard then MIDI Player")
    require("kGenre, kMode, kFeelTexture" in workflow,
            "GENERATE workflow must expose genre, mode and feel")
    require("kPattern, kSynthA, kSynthB, kDrums" in workflow and
            "kSynthAParameters, kSynthBParameters" in workflow,
            "HUB workflow must expose overview, instruments and synth controls")
    require("case WorkflowMode::Song: return kArrange;" in workflow,
            "SONG workflow must resolve to the song editor")
    require("kProject, kGenerator" in workflow and
            "case WorkflowMode::Settings: return kSettingsPages[index];" in workflow,
            "SETTINGS must expose project/setup and advanced generator pages")

    require("#include <M5Cardputer.h>" in workflow and
            "M5Cardputer.Keyboard.keysState().fn" in workflow,
            "hardware workflow navigation must read the physical Fn state")
    require("inline Workspace nextWorkspace(Workspace workspace," in workflow and
            "bool workflowModifier" in workflow,
            "workflow navigation needs an explicit modifier-aware overload")

    next_start = workflow.index("inline Workspace nextWorkspace(Workspace workspace,")
    next_end = workflow.index("inline bool allowsPerformanceKeyboard", next_start)
    next_block = workflow[next_start:next_end]
    require("if (workflowModifier)" in next_block and
            "pageForMode(nextMode(mode, direction))" in next_block,
            "Fn+[ / ] must move to the adjacent workflow")
    require("pageIndexInMode(page) + direction" in next_block and
            "pageAt(mode, nextIndex)" in next_block,
            "plain [ / ] must continue to wrap inside the current workflow")
    require("hardwareWorkflowModifierHeld()" in next_block,
            "the existing display bracket handlers must consume the physical Fn state")

    require("WorkflowPages::nextWorkspace(active_workspace_, 1)" in display and
            "WorkflowPages::nextWorkspace(active_workspace_, -1)" in display,
            "display [ / ] handlers must use page-aware workflow navigation")
    require("WorkflowPages::nextMode(current, direction)" in display and
            "WorkflowPages::pageForMode" in display,
            "Fn+Tab must still switch the five top-level workflows")

    page_dispatch = display.index("currentPage->handleEvent(event)")
    brackets = display.index("if (event.key == ']')", page_dispatch)
    require(page_dispatch < brackets,
            "pages must keep first refusal so local editors can own their controls")
    require("if (e.key == '\\t')" in settings and
            "Group::Timing" in settings and "Group::Notes" in settings and
            "Group::Scale" in settings,
            "plain Tab must retain local Generator subpage navigation")

    for label in ("PERFORM", "GENERATE", "HUB", "SONG", "SETTINGS", "HELP"):
        require(f'return "{label}";' in launcher,
                f"launcher must expose workflow label: {label}")
    require("L/R PAGE" in launcher and "PAGE %d/%d" in launcher,
            "launcher must preview pages inside each workflow")
    require("FN+[ ] WORKFLOW" in launcher and "[ ] PAGE" in launcher,
            "launcher must explain the two navigation levels")


def main() -> None:
    test_theme_selection()
    test_workflow_local_page_navigation()
    print("theme + workflow navigation source regressions: OK")


if __name__ == "__main__":
    main()
