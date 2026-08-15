#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/ui/pages/drum_sequencer_page.cpp"


def require(text: str, needle: str) -> None:
    if needle not in text:
        raise AssertionError(f"{PATH}: missing required drum-engine guard contract: {needle}")


def main() -> None:
    text = PATH.read_text(encoding="utf-8")

    require(text, "activePageIndex() == 1")
    require(text, "auto* feelPage = static_cast<GlobalDrumFeelPage*>(feel.get());")
    require(text, "auto* mainPage = static_cast<DrumSequencerMainPage*>(main.get());")
    require(text, "feelPage->selected_row_ == 0")
    require(text, "mainPage->audio_guard_([&]()")
    require(text, "handled = handleEventLegacy(ui_event);")

    guard_start = text.find("if (activePageIndex() == 1")
    normal_legacy_fallback = text.find(
        "if (activePageIndex() != 0 ||", guard_start
    )
    if guard_start < 0 or normal_legacy_fallback < 0:
        raise AssertionError(f"{PATH}: guarded drum-character path is missing")

    guarded_region = text[guard_start:normal_legacy_fallback]
    if "mainPage->audio_guard_([&]()" not in guarded_region:
        raise AssertionError(
            f"{PATH}: drum Character row must guard legacy engine selection before fallback"
        )

    print("Drum engine AudioGuard source contract: PASS")


if __name__ == "__main__":
    main()
