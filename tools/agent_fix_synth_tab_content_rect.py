#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE = ROOT / "src/ui/pages/tb303_params_page.cpp"
TEST = ROOT / "tests/test_wavemorph_performance_source_regressions.py"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


page = PAGE.read_text(encoding="utf-8")
page = replace_once(
    page,
    "  const auto& content = Layout::CONTENT;\n  drawTabSwitcher(gfx, content);",
    "  const auto& content = Layout::CONTENT;\n"
    "  const Rect contentRect{content.x, content.y, content.w, content.h};\n"
    "  drawTabSwitcher(gfx, contentRect);",
    "tab switcher content geometry",
)
page = replace_once(
    page,
    "    drawMainSummary(gfx, content);",
    "    drawMainSummary(gfx, contentRect);",
    "main summary content geometry",
)
PAGE.write_text(page, encoding="utf-8")


test = TEST.read_text(encoding="utf-8")
old = '''    require('drawSegment(x, "MAIN", !more_tab_)' in page and
            '"MORE", more_tab_' in page and '"TAB >"' in page,
            "MAIN/MORE discoverability must be visible on the parameter page")
'''
new = '''    require('drawSegment(x, "MAIN", !more_tab_)' in page and
            '"MORE", more_tab_' in page and '"TAB >"' in page,
            "MAIN/MORE discoverability must be visible on the parameter page")
    require("const Rect contentRect{content.x, content.y, content.w, content.h}" in page and
            "drawTabSwitcher(gfx, contentRect)" in page and
            "drawMainSummary(gfx, contentRect)" in page,
            "tab helpers must receive explicit Rect geometry on every target")
'''
test = replace_once(test, old, new, "content geometry regression")
TEST.write_text(test, encoding="utf-8")
