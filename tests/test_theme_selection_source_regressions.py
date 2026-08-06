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


def test_perform_piano_key_shapes() -> None:
    visuals = (ROOT / "src/ui/components/music_visuals.h").read_text(
        encoding="utf-8"
    )

    row_start = visuals.index("inline void drawPianoKeyRow")
    row_end = visuals.index("inline void drawPiano(", row_start)
    row = visuals[row_start:row_end]

    require("isBlackPianoPitch(note)" in row and
            "const int blackH" in row and "const int blackW" in row,
            "melodic rows must retain long white keys with shorter black keys")
    require("drawTinyNoteLabel" in row and "keyboard.noteForKey" in row,
            "piano keys must show compact resolved note names")
    require("keyboard.isPhysicalKeyHeld(physical)" in row,
            "two piano rows must preserve independent physical-key held state")
    require("char keyLabel" not in row and "gfx.drawText" not in row,
            "piano rows must not print Cardputer key letters or large 5x7 labels")
    require('constexpr char kUpperRow[] = "qwertyuiop";' in visuals and
            'constexpr char kLowerRow[] = "asdfghjkl";' in visuals,
            "both physical note rows must remain represented")
    require("tinyGlyph" in visuals and "gfx.fillRect(cursorX + column" in visuals,
            "compact note labels must remain immediate-mode and allocation-free")


def test_workflow_local_page_navigation() -> None:
    workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    display_h = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
    launcher = (ROOT / "src/ui/workspace_launcher_overlay.h").read_text(encoding="utf-8")
    feel = (ROOT / "src/ui/pages/settings_page.cpp").read_text(encoding="utf-8")

    for mode in ("Perform", "Generate", "Hub", "Song", "Settings"):
        require(f"WorkflowMode::{mode}" in workflow,
                f"missing top-level workflow: {mode}")
    require("constexpr int count = 5;" in workflow,
            "Fn+Tab must remain a bounded five-workflow ring")

    require("kPerform, kPlayer" in workflow,
            "PERFORM workflow must contain keyboard then MIDI Player")
    require("kGenre, kGenerator, kMode, kFeelTexture" in workflow,
            "GENERATE must expose GENRE, FEEL, GENERATION and TEXTURE")
    require("case WorkflowMode::Generate: return 4;" in workflow,
            "GENERATE workflow must have four fixed page addresses")
    require("kPattern, kSynthA, kSynthB, kDrums" in workflow and
            "kSynthAParameters, kSynthBParameters" in workflow,
            "HUB workflow must expose overview, instruments and synth controls")
    require("case WorkflowMode::Song: return kArrange;" in workflow,
            "SONG workflow must resolve to the song editor")
    require("static constexpr int kSettingsPages[] = {\n        kProject," in workflow and
            "case WorkflowMode::Settings: return 1;" in workflow,
            "SETTINGS must contain only project/setup after FEEL moved to GENERATE")

    require("#include <M5Cardputer.h>" in workflow and
            "M5Cardputer.Keyboard.keysState().fn" in workflow,
            "hardware workflow navigation must read the physical Fn state")
    session = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
    require("workflowNavigationTarget" in session and
            "rememberedAdjacentWorkflowPage" in session,
            "workflow changes must resolve through per-workflow page memory")
    require("void switchWorkflow_(int direction);" in display_h and
            "rememberedAdjacentWorkflowPage" in display and
            display.count("switchWorkflow_(") >= 6,
            "Fn+Tab and Fn brackets must share one remembered-page route")
    require("pageForMode(\n                WorkflowPages::nextMode" not in display,
            "workflow switches must not reset to the first page")

    page_dispatch = display.index("currentPage->handleEvent(event)")
    fn_left = display.index("event.meta && (event.key == '['")
    fn_right = display.index("event.meta && (event.key == ']'")
    brackets = display.index("if (event.key == ']')", page_dispatch)
    require(fn_left < page_dispatch and fn_right < page_dispatch,
            "Fn brackets must bypass page-local first refusal")
    require(page_dispatch < brackets,
            "plain brackets must still give local editors first refusal")
    require("UIInput::isTab(event)" in feel and
            "FocusRow::Swing" in feel and "FocusRow::TimingHumanize" in feel and
            "FocusRow::VelocityHumanize" in feel,
            "plain Tab must navigate the FEEL timing/velocity rows")

    for label in ("PERFORM", "GENERATE", "HUB", "SONG", "SETTINGS", "HELP"):
        require(f'return "{label}";' in launcher,
                f"launcher must expose workflow label: {label}")
    require("L/R PAGE" in launcher and "PAGE %d/%d" in launcher,
            "launcher must preview pages inside each workflow")
    require("FN+[ ] WORKFLOW" in launcher and "[ ] PAGE" in launcher,
            "launcher must explain the two navigation levels")
    require("GROOVEPUTER / NAV R3" in launcher and
            "MEM %d %d %d %d %d" in launcher,
            "hardware retest must expose build revision and workflow memory")


def main() -> None:
    test_theme_selection()
    test_perform_piano_key_shapes()
    test_workflow_local_page_navigation()
    print("theme + workflow navigation source regressions: OK")


if __name__ == "__main__":
    main()
