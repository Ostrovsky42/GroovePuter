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
    require('"Alt+K       Sampler"' in help_source and
            '"=== SAMPLER ==="' in help_source,
            "SAMPLER shortcut and page help must stay visible")


if __name__ == "__main__":
    main()
