#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEOMETRY = (ROOT / "src/ui/screen_geometry.h").read_text(encoding="utf-8")
LAYOUT = (ROOT / "src/ui/layout_manager.cpp").read_text(encoding="utf-8")
PERFORM = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("constexpr LayoutRect CONTENT = {0, 16, 240, 93}" in GEOMETRY,
            "standard surface body must be the non-overlapping 93px zone")
    require("0, FOOTER.y - PERFORMANCE_HUD_H, SCREEN_W, PERFORMANCE_HUD_H" in GEOMETRY,
            "performance strip must remain the existing shell-owned zone before footer")

    require("gfx.fillRect(Layout::CONTENT.x, Layout::CONTENT.y, Layout::CONTENT.w, Layout::CONTENT.h" in LAYOUT,
            "clearContent must derive its extent from the canonical body rectangle")
    require("PERFORMANCE_HUD" not in LAYOUT.split("void LayoutManager::clearContent", 1)[1].split("}", 1)[0],
            "clearContent must not know about or compensate for the HUD")

    # U1E is not a density redesign. Existing line-7 product content remains;
    # a later checkpoint may remove the historical manual offset with visual evidence.
    require("LayoutManager::lineY(7) - 2" in PERFORM,
            "U1E must not silently rearrange the preserved PERFORM composition")


if __name__ == "__main__":
    main()
