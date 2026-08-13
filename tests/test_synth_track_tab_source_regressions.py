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
    require("constexpr int kTabStripX = 72;" in source,
            "compact synth tabs must occupy the former PTRN label slot")
    require("constexpr int kParamsTabStripX = 42;" in source,
            "KNOBS/MORE indicator must stay left of the child MAIN/MORE switcher")
    require("constexpr int kTabStripW = 32;" in source,
            "compact synth tabs must stop before the pattern-number row")
    require("kTabStripX + kTabStripW <= kPatternNumbersX" in source,
            "synth tabs must prove that they cannot cover pattern numbers")
    require("notesTab &&" in source and
            "UI::currentStyle != VisualStyle::RETRO_CLASSIC" in source and
            "UI::currentStyle != VisualStyle::AMBER" in source,
            "MINIMAL NOTES must suppress tabs instead of covering pattern numbers")
    require("synth_tab_ == SynthTab::Notes ? kTabStripX : kParamsTabStripX" in source,
            "tab x-position must follow the active child layout")
    require('"[NOTES] KNOBS MORE"' not in source and
            '"NOTES [KNOBS] MORE"' not in source and
            '"NOTES KNOBS [MORE]"' not in source,
            "wide synth tab labels must not return over pattern controls")
    require('"[TAB]NOTES [U/D]ROW [L/R]CHANGE"' in source,
            "MORE footer must describe the new return-to-NOTES behavior")


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
