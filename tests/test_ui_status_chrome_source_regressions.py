#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
PATTERN_EDITOR = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # UI Constitution U1A: presentation text and geometry are not semantic APIs.
    require("statusContextForTitle" not in SOURCE,
            "status chrome must not derive semantic context from display titles")
    require("titleContains(" not in SOURCE,
            "display-title substring parsing must not be a semantic dependency")
    require("statusContextForTitle(UI::activePageTitle())" not in SOURCE,
            "global chrome must not reconstruct context from active-page text")
    require("publishActivePageTitle(getTitle().c_str())" not in CORE,
            "IPage::setBoundaries must be geometry-only")

    # The top-level navigation owner projects canonical page identity into a
    # typed UI location and uses the resulting context when it captures the
    # bounded frame-status projection. Rendering receives only that snapshot.
    require("tryUiLocationForPage(page_index_" in DISPLAY,
            "MiniAcidDisplay must project canonical navigation into typed UI location")
    require("uiStatusContextForLocation(" in DISPLAY,
            "status context must be derived from typed UI location")
    require("captureUiStatusSnapshot(mini_acid_, statusContext)" in DISPLAY,
            "global status capture must receive explicit typed context")
    require("drawLiveMixLockBadge(gfx_, frameStatus)" in DISPLAY,
            "global status rendering must consume the captured frame snapshot")
    require("status.context = context;" in SOURCE,
            "status snapshot must receive the explicit semantic context")
    require("drawStatusChrome(gfx, status);" in SOURCE,
            "the compatibility overlay hook must render the captured snapshot")

    # Existing unrelated pattern-address and Song invariants remain preserved.
    require("patternAddressFromParts(" in SOURCE and
            "miniAcid.currentPageIndex(), bank, slot" in SOURCE,
            "currentPageIndex is allowed only as the page part of a pattern address")
    require("displayDrumLocalPatternIndex()" in SOURCE,
            "drum status address must use a local slot, never a global pattern ID")
    require("status.context == UiStatusContext::Player" in SOURCE,
            "stopped SMF visibility must depend on the real MIDI Player context")
    require("UI::songPatternPageShortcut(" in SONG,
            "Song must use the canonical 16-page shortcut mapper")
    require("songPatternFromPageBankIndex(mini_acid_.currentPageIndex(), bankIndex, patternIdx)" in SONG,
            "Song slot assignment must preserve the current PAGE and target-track BANK")
    require("formatPatternAddressParts(address, sizeof(address)," in PATTERN_EDITOR,
            "note editor must format the canonical composite pattern address")
    require("mini_acid_.currentPageIndex(), bank_index_, selectedPattern" in PATTERN_EDITOR,
            "note editor composite ID must use current PAGE, BANK, and SLOT")


if __name__ == "__main__":
    main()
