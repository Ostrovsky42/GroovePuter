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
    require('"[NOTES] KNOBS MORE"' in source,
            "NOTES indicator must be visible")
    require('"NOTES [KNOBS] MORE"' in source,
            "KNOBS indicator must be visible")
    require('"NOTES KNOBS [MORE]"' in source,
            "MORE indicator must be visible")
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


if __name__ == "__main__":
    test_synth_track_owns_notes_knobs_more_cycle()
    test_hub_hides_legacy_sound_pages_but_keeps_ids_compatible()
    print("synth track tab source regressions: PASS")
