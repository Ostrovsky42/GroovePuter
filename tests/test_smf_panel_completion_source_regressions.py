#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_block(source: str, start_token: str, end_token: str) -> str:
    start = source.index(start_token)
    end = source.index(end_token, start)
    return source[start:end]


def main() -> None:
    page_h = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")
    page_cpp = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(encoding="utf-8")
    session = (ROOT / "src/ui/pages/smf_player_session_state.h").read_text(encoding="utf-8")
    redraw = (ROOT / "src/ui/pages/smf_player_redraw.h").read_text(encoding="utf-8")
    layout = (ROOT / "src/ui/layout_manager.cpp").read_text(encoding="utf-8")

    require("SmfPlayerSessionBinding sessionBinding_" in page_h,
            "SMF page must bind bounded state to its object lifetime")
    require("void onExit() override { sessionBinding_.setActive(false); }" in page_h,
            "cached previous pages must stop intercepting redraw after exit")
    require(page_h.count("SmfPlayerTrackedFlag") == 3,
            "browser, performance and inspector visibility must be session tracked")
    for token in (
        "currentPath_",
        "selection_",
        "scroll_",
        "channelInspectorScroll_",
    ):
        require(token in page_h, f"session binding must retain {token}")

    require("bool valid{false};" in session and
            "activationEpoch_" in session and
            "ignoreNextAssignment_" in session,
            "session restore must distinguish first entry from a restored page")
    require("~SmfPlayerSessionBinding()" in session and
            "publish();" in session and
            "setActive(false)" in session,
            "page eviction must publish the final bounded session")
    for forbidden in ("std::vector", "std::map", "new ", "malloc("):
        require(forbidden not in session,
                f"session state must remain bounded: {forbidden}")

    require("interceptSmfPlayerContentClear" in layout,
            "LayoutManager must delegate the active SMF page clear policy")
    require("if (!session.active()) return false;" in redraw,
            "partial redraw must never affect another active page")
    require("activationEpoch != activationEpoch_" in redraw,
            "re-entering the page must force one safe full frame")
    require("clearLine(gfx, 3);" in redraw,
            "progress and wave animation must refresh only its bounded row")
    require("current.currentTick != previous_.currentTick" in redraw and
            "clearLine(gfx, 4);" in redraw,
            "progress text must invalidate its own row instead of full content")
    require("Layout::CONTENT.w" in redraw and
            "Layout::LINE_HEIGHT + 2" in redraw,
            "dirty clears must use bounded content-row rectangles")

    draw_block = function_block(
        page_cpp,
        "void SmfPlayerPage::drawHeader",
        "void SmfPlayerPage::ensureSelectionVisible",
    )
    for forbidden in (
        "SD.open",
        "openNextFile",
        "ensureCardputerSdMounted",
        "SD.mkdir",
        "refreshFiles()",
        "resolveEntry(",
    ):
        require(forbidden not in draw_block,
                f"SMF draw path must not traverse storage: {forbidden}")
    require("browserRows_" in page_h and "kBrowserVisibleRows = 7" in page_h,
            "browser rendering must use the existing bounded visible-row cache")
    require(not (ROOT / "src/ui/pages/smf_directory_cache.h").exists(),
            "completion must not retain an unused second directory-cache architecture")

    require("BrowserDiagnosticSink" in page_h and
            "inline static constexpr BrowserDiagnosticSink Serial" in page_h,
            "routine page-local browser UART logging must be suppressed")
    require("::Serial" not in page_cpp,
            "SMF page must not bypass its page-local diagnostic policy")

    guard_block = page_h[page_h.index("template <typename F>"):]
    require("gfx" not in guard_block and "draw" not in guard_block,
            "AudioGuard helper must not contain UI drawing")

    print("SMF panel completion source regressions: OK")


if __name__ == "__main__":
    main()
