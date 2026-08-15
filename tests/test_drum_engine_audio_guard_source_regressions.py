#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP_PATH = ROOT / "src/ui/pages/drum_sequencer_page.cpp"
HEADER_PATH = ROOT / "src/ui/pages/drum_sequencer_page.h"


def require(text: str, needle: str, path: Path) -> None:
    if needle not in text:
        raise AssertionError(f"{path}: missing required Drum Sequencer realtime contract: {needle}")


def main() -> None:
    cpp = CPP_PATH.read_text(encoding="utf-8")
    header = HEADER_PATH.read_text(encoding="utf-8")

    # Character-row engine replacement owns a DrumSynthVoice. It must execute
    # while AudioTask is paused at an existing AudioGuard block boundary.
    require(cpp, "activePageIndex() == 1", CPP_PATH)
    require(cpp, "auto* feelPage = static_cast<GlobalDrumFeelPage*>(feel.get());", CPP_PATH)
    require(cpp, "auto* mainPage = static_cast<DrumSequencerMainPage*>(main.get());", CPP_PATH)
    require(cpp, "feelPage->selected_row_ == 0", CPP_PATH)
    require(cpp, "mainPage->audio_guard_([&]()", CPP_PATH)
    require(cpp, "handled = handleEventLegacy(ui_event);", CPP_PATH)

    guard_start = cpp.find("if (activePageIndex() == 1")
    normal_legacy_fallback = cpp.find("if (activePageIndex() != 0 ||", guard_start)
    if guard_start < 0 or normal_legacy_fallback < 0:
        raise AssertionError(f"{CPP_PATH}: guarded drum-character path is missing")
    guarded_region = cpp[guard_start:normal_legacy_fallback]
    if "mainPage->audio_guard_([&]()" not in guarded_region:
        raise AssertionError(
            f"{CPP_PATH}: drum Character row must guard legacy engine selection before fallback"
        )

    # The retained Minimal implementation explicitly draws grid_component_ and
    # then calls its Container parent. Because the grid is already that
    # container's child, the wrapper must suppress only the duplicate child pass
    # while main tab 0 is active. Other tabs still need normal child drawing.
    require(header, "void draw(IGfx& gfx) override;", HEADER_PATH)
    require(cpp, "bool g_suppressPatternLockedChildDraw = false;", CPP_PATH)
    require(cpp, "if (g_suppressPatternLockedChildDraw) return;", CPP_PATH)
    require(cpp, "void DrumSequencerPage::draw(IGfx& gfx)", CPP_PATH)
    require(cpp, "g_suppressPatternLockedChildDraw = (activePageIndex() == 0);", CPP_PATH)
    require(cpp, "DrumSequencerLegacyMultiPage::draw(gfx);", CPP_PATH)
    require(cpp, "g_suppressPatternLockedChildDraw = previous;", CPP_PATH)

    print("Drum Sequencer realtime UI source contracts: PASS")


if __name__ == "__main__":
    main()
