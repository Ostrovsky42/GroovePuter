#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON_H = (ROOT / "src/ui/ui_common.h").read_text(encoding="utf-8")
COMMON_CPP = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # U1C: status derivation is an explicit bounded capture step. Rendering the
    # chrome must not perform a second live MiniAcid read after page drawing.
    require("captureUiStatusSnapshot" in COMMON_H,
            "ui_common has no explicit status snapshot capture API")
    require("captureUiStatusSnapshot" in COMMON_CPP,
            "status snapshot capture is not implemented")
    require("drawStatusChrome(IGfx& gfx, const UiStatusSnapshot& status)" in COMMON_CPP,
            "status chrome is not a pure renderer of a captured snapshot")
    require("drawStatusChrome(IGfx& gfx, MiniAcid&" not in COMMON_CPP,
            "status chrome still re-reads live MiniAcid state while rendering")

    require("const UI::UiStatusSnapshot frameStatus" in DISPLAY,
            "MiniAcidDisplay does not own one bounded status snapshot per frame")
    require("UI::captureUiStatusSnapshot(mini_acid_, statusContext)" in DISPLAY,
            "MiniAcidDisplay does not capture status from authoritative state")
    require("UI::drawLiveMixLockBadge(gfx_, frameStatus)" in DISPLAY,
            "global chrome does not render the captured frame snapshot")
    require("drawLiveMixLockBadge(gfx_, mini_acid_" not in DISPLAY,
            "MiniAcidDisplay still asks chrome to re-read live engine state")

    capture = DISPLAY.index("UI::captureUiStatusSnapshot(mini_acid_, statusContext)")
    page_draw = DISPLAY.index("currentPage->draw(gfx_)")
    chrome_draw = DISPLAY.index("UI::drawLiveMixLockBadge(gfx_, frameStatus)")
    require(capture < page_draw < chrome_draw,
            "frame status must be captured once before body draw and reused after it")

    # Resource Law: the new coherent semantic view must remain tiny. U1C must
    # not introduce full Pattern/Phrase/framebuffer copies into MiniAcidDisplay.
    require("currentPhraseBuffer" not in DISPLAY,
            "U1C frame snapshot must not copy Phrase material into the shell")
    require("frame_" not in DISPLAY and "CardputerDisplay" not in DISPLAY,
            "U1C frame snapshot must stay renderer-backend neutral")


if __name__ == "__main__":
    main()
