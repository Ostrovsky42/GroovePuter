#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON_H = (ROOT / "src/ui/ui_common.h").read_text(encoding="utf-8")
COMMON_CPP = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")
DISPLAY_CPP = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
SHELL_H = ROOT / "src/ui/ui_shell_frame.h"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_block(source: str, signature: str, next_signature: str) -> str:
    start = source.find(signature)
    require(start >= 0, f"missing function: {signature}")
    end = source.find(next_signature, start)
    require(end > start, f"cannot bound function: {signature}")
    return source[start:end]


def main() -> None:
    require(SHELL_H.exists(),
            "U1F needs an explicit stack-bounded shell frame model")
    shell = SHELL_H.read_text(encoding="utf-8")
    require("struct UiFooterModel" in shell and "struct UiShellFrameModel" in shell,
            "U1F needs typed footer and shell-frame values")
    require("char left[64]" in shell and "char right[64]" in shell,
            "U1F footer copies must remain bounded to 64 bytes per side")
    require("static_assert(sizeof(UiShellFrameModel) <= 136" in shell,
            "U1F shell frame stack budget must stay explicit")

    header = function_block(COMMON_CPP,
                            "void drawStandardHeader(",
                            "void drawStatusChrome(")
    require("LayoutManager::drawHeader" not in header,
            "page standard-header helper must not own global header pixels")
    require("mini_acid." not in header,
            "page standard-header helper must not re-read live engine truth")

    footer = function_block(COMMON_CPP,
                            "void drawStandardFooter(",
                            "void drawVerticalList(")
    require("LayoutManager::drawFooter" not in footer,
            "page standard-footer helper must publish a model, not draw pixels")
    require("publishShellFooter" in footer,
            "page standard-footer helper must publish the effective footer model")

    require("beginShellFrameModel" in COMMON_H and
            "endShellFrameModel" in COMMON_H and
            "drawShellFooter" in COMMON_H,
            "U1F shell frame lifecycle must be explicit in the public UI boundary")

    require("LayoutManager::drawHeader" not in DISPLAY_CPP,
            "MiniAcidDisplay must not keep a second legacy header renderer")
    require("LayoutManager::drawFooter" not in DISPLAY_CPP,
            "MiniAcidDisplay invalid-page path must use the same shell footer owner")
    require("drawLiveMixLockBadge" not in DISPLAY_CPP,
            "U1F must call the canonical status-chrome renderer directly")

    begin = DISPLAY_CPP.find("UI::beginShellFrameModel(shellFrame)")
    page = DISPLAY_CPP.find("currentPage->draw(gfx_)")
    end = DISPLAY_CPP.find("UI::endShellFrameModel()")
    chrome = DISPLAY_CPP.find("UI::drawStatusChrome(gfx_, frameStatus)")
    footer_draw = DISPLAY_CPP.find("UI::drawShellFooter(gfx_, shellFrame.footer)")
    hud = DISPLAY_CPP.find("UI::drawPerformanceHud(gfx_, mini_acid_")
    require(min(begin, page, end, chrome, footer_draw, hud) >= 0,
            "U1F shell lifecycle/render calls are incomplete")
    require(begin < page < end < chrome < footer_draw < hud,
            "U1F order must be publish during page draw, then shell chrome/footer, then HUD")

    shell_footer = function_block(COMMON_CPP,
                                  "void drawShellFooter(",
                                  "void drawVerticalList(")
    require("LayoutManager::drawFooter" in shell_footer,
            "exactly the shell footer renderer may paint footer pixels")


if __name__ == "__main__":
    main()
