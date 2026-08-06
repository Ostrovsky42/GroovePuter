#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("statusContextForTitle" in SOURCE,
            "status chrome must derive context from the active UI page title")
    require("gStatusContext = statusContextForTitle(title);" in SOURCE,
            "drawStandardHeader must publish the active UI context")
    require("publishActivePageTitle(getTitle().c_str())" in CORE,
            "top-level pages must publish their current title in every visual style")
    require("statusContextForTitle(UI::activePageTitle())" in SOURCE,
            "status chrome must refresh context from the top-level page title")
    require("status.context = statusContextForTitle" not in SOURCE,
            "pattern-page storage index must never replace the UI page context")
    require("patternAddressFromParts(" in SOURCE and
            "miniAcid.currentPageIndex(), bank, slot" in SOURCE,
            "currentPageIndex is allowed only as the page part of a pattern address")
    require("displayDrumLocalPatternIndex()" in SOURCE,
            "drum status address must use a local slot, never a global pattern ID")
    require("status.context == UiStatusContext::Player" in SOURCE,
            "stopped SMF visibility must depend on the real MIDI Player context")
    require("drawStatusChrome(gfx, mini_acid);" in SOURCE,
            "the existing global overlay hook must render the status chrome")


if __name__ == "__main__":
    main()
