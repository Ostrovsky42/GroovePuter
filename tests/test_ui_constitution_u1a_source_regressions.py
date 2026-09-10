#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")
COMMON = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # UI Constitution V1: geometry may not publish presentation/semantic state.
    require("publishActivePageTitle(getTitle().c_str())" not in CORE,
            "IPage::setBoundaries still publishes active-page title")

    # Presentation strings are not semantic API.
    require("statusContextForTitle" not in COMMON,
            "status chrome still derives semantic context from presentation title")
    require("titleContains(" not in COMMON,
            "status chrome still parses presentation strings for semantics")
    require("activePageTitle()" not in COMMON,
            "status chrome still reads global presentation title as semantic input")

    # The top-level navigation owner must explicitly project typed UI location/context.
    require("tryUiLocationForPage" in DISPLAY,
            "MiniAcidDisplay does not project canonical page navigation into typed UI location")
    require("uiStatusContextForLocation" in DISPLAY,
            "MiniAcidDisplay does not derive status context from typed UI location")

    # Resource Law: this semantic slice must stay renderer-backend neutral.
    require("CardputerDisplay" not in CORE and "frame_" not in CORE,
            "core UI semantic API must not depend on the Cardputer full framebuffer")


if __name__ == "__main__":
    main()
