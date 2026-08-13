#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_synth_track_owns_notes_knobs_more_cycle() -> None:
    source = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(
        encoding="utf-8"
    )
    params = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(
        encoding="utf-8"
    )
    pattern = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(
        encoding="utf-8"
    )
    geometry = (ROOT / "src/ui/screen_geometry.h").read_text(encoding="utf-8")
    common = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/ui/pages/synth_sequencer_page.h").read_text(
        encoding="utf-8"
    )

    require("enum class SynthTab" in header,
            "synth track must own an explicit local tab state")
    require("Notes = 0" in header and "Knobs" in header and "More" in header,
            "synth local hierarchy must contain NOTES, KNOBS and MORE")
    require("std::make_shared<PatternEditPage>" in source,
            "NOTES must keep the existing PatternEditPage")
    require("std::make_shared<TB303ParamsPage>" in source,
            "KNOBS/MORE must reuse the existing synth params page")
    require("GlobalSynthFeelPage" not in source,
            "duplicate technical synth settings page must be removed")
    require("(static_cast<int>(synth_tab_) + 1) % 3" in source,
            "plain Tab must cycle exactly three local synth states")
    require('"[N]KM"' in source,
            "compact NOTES indicator must be visible")
    require('"N[K]M"' in source,
            "compact KNOBS indicator must be visible")
    require('"NK[M]"' in source,
            "compact MORE indicator must be visible")
    require("constexpr int kNotesTabStripX = 190;" in source,
            "NOTES tab must appear after the pattern-number row")
    require("constexpr int kParamsTabStripX = 172;" in source,
            "params tab must sit near the right edge before its mode label")
    require("constexpr int kTabStripW = 32;" in source,
            "compact synth tabs must stop before the pattern-number row")
    require("kNotesTabStripX > kPatternNumbersEndX" in source and
            "kNotesTabStripX + kTabStripW <= Layout::SCREEN_W" in source,
            "NOTES tab must prove it is after pattern numbers and on screen")
    require("notesTab &&" in source and
            "UI::currentStyle != VisualStyle::RETRO_CLASSIC" in source and
            "UI::currentStyle != VisualStyle::AMBER" in source,
            "MINIMAL NOTES must suppress tabs instead of covering pattern numbers")
    require("notesTab ? kNotesTabStripX : kParamsTabStripX" in source,
            "tabs must use their respective right-side safe slots")
    require('"[NOTES] KNOBS MORE"' not in source and
            '"NOTES [KNOBS] MORE"' not in source and
            '"NOTES KNOBS [MORE]"' not in source,
            "wide synth tab labels must not return over pattern controls")
    require("drawTabSwitcher" not in params and '"MAIN"' not in params,
            "params page must not duplicate the parent N/K/M switcher")
    require("UIInput::isTab(ui_event)" not in params,
            "only SynthSequencerPage may own the three-state Tab cycle")
    require("UI::drawStandardFooter" not in source,
            "parent must not redraw the params footer")
    require('"[TAB]NOTES [U/D]ROW [L/R]CHANGE"' in params,
            "MORE footer must describe the new return-to-NOTES behavior")
    require('gfx.textWidth("TAB >")' not in params,
            "summary must not duplicate the footer Tab hint")
    require("constexpr LayoutRect PERFORMANCE_HUD" in geometry,
            "global performance overlays need a reserved layout strip")
    require("constexpr LayoutRect PERFORMANCE_WAVEFORM" in geometry,
            "waveform needs a non-overlapping slot inside the HUD strip")
    require(common.count("const int y = Layout::PERFORMANCE_HUD.y;") == 2,
            "Feel and Mutes overlays must share the reserved strip")
    require("Layout::PERFORMANCE_WAVEFORM.h" in common and
            "Layout::PERFORMANCE_WAVEFORM.y" in common and
            "Layout::PERFORMANCE_WAVEFORM.x" in common and
            "Layout::PERFORMANCE_WAVEFORM.w" in common,
            "waveform must remain inside its reserved HUD slot")
    require("gfx.fillRect(Layout::PERFORMANCE_HUD.x" in source and
            "palette.background" in source,
            "synth pages must clear their reserved HUD strip before overlays")
    require("synth summary must stay above the performance HUD" in params and
            "synth MORE rows must stay above the performance HUD" in params,
            "params layout must prove that it cannot enter the HUD strip")
    require("std::min(y + h - 2, Layout::PERFORMANCE_HUD.y)" in pattern,
            "minimal NOTES grid must stop above the performance HUD")
    require(pattern.count(
                "std::min(y + h - 12, Layout::PERFORMANCE_HUD.y) - contentY"
            ) == 2,
            "retro and amber NOTES grids must stop above the performance HUD")


def test_hub_hides_legacy_sound_pages_but_keeps_ids_compatible() -> None:
    workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")

    require("constexpr int kSynthAParameters = 3" in workflow,
            "legacy Synth A sound page id must remain defined")
    require("constexpr int kSynthBParameters = 4" in workflow,
            "legacy Synth B sound page id must remain defined")
    require("if (page == kSynthAParameters) return kSynthA;" in workflow,
            "legacy Synth A sound sessions must normalize to SYNTH A")
    require("if (page == kSynthBParameters) return kSynthB;" in workflow,
            "legacy Synth B sound sessions must normalize to SYNTH B")
    require("case WorkflowMode::Hub: return 4;" in workflow,
            "HUB must expose four top-level pages after sound-page collapse")
    require("kPattern, kSynthA, kSynthB, kDrums" in workflow,
            "HUB page ring must be OVERVIEW, SYNTH A, SYNTH B, DRUMS")
    require('return "SYNTH A SOUND"' not in workflow and
            'return "SYNTH B SOUND"' not in workflow,
            "standalone SOUND labels must not remain in top-level navigation")

    ownership_start = workflow.index("inline bool allowsPerformanceKeyboard")
    ownership = workflow[ownership_start:]
    require("if (page == kSynthAParameters) return true;" in ownership,
            "legacy Synth A SOUND input ownership must remain compatible")
    require("return normalizeLegacyPage(page) == kPerform;" in ownership,
            "normal SYNTH A/B editor pages must not acquire performance-keyboard ownership")
    require("page == kSynthA || page == kSynthB" not in ownership,
            "track collapse must not steal letter keys from the note editor")


if __name__ == "__main__":
    test_synth_track_owns_notes_knobs_more_cycle()
    test_hub_hides_legacy_sound_pages_but_keeps_ids_compatible()
    print("synth track tab source regressions: PASS")
