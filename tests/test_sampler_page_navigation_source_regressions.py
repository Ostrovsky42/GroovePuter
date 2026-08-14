#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    config = (ROOT / "src/ui/ui_config.h").read_text(encoding="utf-8")
    session = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
    workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    page_h = (ROOT / "src/ui/pages/sampler_page.h").read_text(encoding="utf-8")
    page_cpp = (ROOT / "src/ui/pages/sampler_page.cpp").read_text(encoding="utf-8")
    help_source = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")

    require("static constexpr int kPageCount = 16;" in config,
            "SAMPLER page must have a valid standalone UI page slot")
    require("constexpr int kSampler = 15;" in session and
            "constexpr int kSampler = 15;" in workflow,
            "SAMPLER page id must remain stable across UI/session layers")
    require("case WorkflowPages::kSampler:" in display and
            "std::make_unique<SamplerPage>" in display,
            "SAMPLER page must be constructible through the real display path")
    require("event.key == 'k' || event.key == 'K'" in display and
            "goToPage(WorkflowPages::kSampler)" in display,
            "Alt+K must remain the direct SAMPLER entry point")
    require("isStandalonePage(index)" in display and
            "isStandalonePage(page_index_)" in display,
            "standalone SAMPLER navigation must not overwrite workflow memory")

    require("static constexpr int kRecoveredPadCount = 8;" in page_h,
            "0.9.3 user sampler workflow must stay bounded to sequenced pads 1..8")
    require('constexpr char kSequencedPadKeys[] = "qwertyui";' in page_cpp,
            "direct sampler trigger mapping must expose exactly Q W E R T Y U I")
    require("GROOVEPUTER_LEFT" in page_cpp and "adjustFocusedElement(-1)" in page_cpp,
            "SamplerPage must support backward selection/adjustment")
    require("GROOVEPUTER_RIGHT" in page_cpp and "adjustFocusedElement(1)" in page_cpp,
            "SamplerPage must support forward selection/adjustment")
    require("kit_ctrl_" not in page_h and "openLoadKitDialog" not in page_cpp and
            '"/bonnethead/kits"' not in page_cpp,
            "unsafe historical KIT LOAD must remain outside 0.9.3")

    require('"Alt+K       Sampler"' in help_source and
            '"=== SAMPLER ==="' in help_source,
            "SAMPLER shortcut and page help must stay visible")
    require('"Q W E R T Y U I  Pads 1..8"' in help_source,
            "help must document the exact eight direct trigger keys")
    require('"KIT LOAD    Deferred to 0.9.4"' in help_source,
            "help must state the 0.9.3 kit boundary explicitly")


if __name__ == "__main__":
    main()
