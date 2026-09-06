#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTINUITY = ROOT / "src/ui/ui_view_continuity.h"
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")
DISPLAY_H = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
DISPLAY_CPP = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
SYNTH_H = (ROOT / "src/ui/pages/synth_sequencer_page.h").read_text(encoding="utf-8")
PERFORM_H = (ROOT / "src/ui/pages/perform_page.h").read_text(encoding="utf-8")
FEEL_H = (ROOT / "src/ui/pages/feel_page.h").read_text(encoding="utf-8")
GENRE_H = (ROOT / "src/ui/pages/genre_page.h").read_text(encoding="utf-8")
SESSION = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
PATTERN_H = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # Characterization: renderer objects are explicitly evictable. U1D does
    # not change this policy; it removes musician continuity from that lifetime.
    require("pages_[i].reset()" in DISPLAY_CPP,
            "U1D must preserve the existing lazy page eviction path")
    require("freeDRAM < 16384" in DISPLAY_CPP,
            "U1D must not silently change the current Cardputer eviction threshold")

    # Current renderer-local defaults document the old failure: recreation can
    # reset useful position even though musical state has not changed.
    require("SynthTab synth_tab_ = SynthTab::Notes" in SYNTH_H,
            "Synth local representation must remain an explicit continuity concern")
    require("PerformanceToolContext selectedContext_{PerformanceToolContext::Key}" in PERFORM_H,
            "PERFORM tool context must remain an explicit continuity concern")
    require("FocusRow focus_ = FocusRow::Profile" in FEEL_H,
            "FEEL focus must remain an explicit continuity concern")
    require("FocusRow focus_ = FocusRow::Genre" in GENRE_H,
            "GENRE focus must remain an explicit continuity concern")

    # GREEN architecture: one tiny runtime-only shell owner. This file is
    # intentionally absent at RED.
    require(CONTINUITY.exists(),
            "U1D needs a fixed-size runtime view-continuity value outside page residency")
    continuity = CONTINUITY.read_text(encoding="utf-8")
    require("struct UiViewContinuityState" in continuity,
            "U1D continuity must be an explicit typed value")
    require("static_assert(sizeof(UiViewContinuityState) <= 16" in continuity,
            "U1D continuity payload must remain <= 16 bytes")

    # U4A adds only a coarse Phrase *view* focus. It is valid residency
    # continuity, not Phrase material or mutation-sensitive event selection.
    require("uint8_t phraseFocusBar[2]{0, 0};" in continuity,
            "Phrase bar focus must survive Synth renderer eviction per voice")
    material_free_continuity = continuity.replace("phraseFocusBar[2]", "viewFocus[2]")
    require("pattern" not in material_free_continuity.lower() and
            "phrase" not in material_free_continuity.lower(),
            "continuity must not copy Pattern/Phrase material or editor selection")

    require('#include "ui_view_continuity.h"' in DISPLAY_H,
            "MiniAcidDisplay must see the runtime continuity type directly")
    require("UI::UiViewContinuityState ui_view_continuity_" in DISPLAY_H,
            "MiniAcidDisplay must own the runtime continuity value")
    require("captureViewContinuity" in CORE and "restoreViewContinuity" in CORE,
            "IPage must expose bounded capture/restore hooks without shell RTTI")

    # Persistence and renderer continuity are deliberately different lifetime
    # domains. The existing tiny persisted schema must not absorb U1D fields.
    require("UiViewContinuityState" not in SESSION,
            "U1D must not expand persistent UiSessionState")
    require("static_assert(sizeof(UiSessionState) <= 12" in SESSION,
            "existing UI persistence budget must remain unchanged")

    # The lifetime boundary is the behavior: snapshot immediately before object
    # destruction, then restore immediately after recreation.
    capture = DISPLAY_CPP.find("pages_[i]->captureViewContinuity(ui_view_continuity_)")
    reset = DISPLAY_CPP.find("pages_[i].reset()")
    create = DISPLAY_CPP.find("pages_[index] = createPage_(index)")
    restore = DISPLAY_CPP.find("pages_[index]->restoreViewContinuity(ui_view_continuity_)")
    require(capture >= 0 and reset >= 0 and capture < reset,
            "eviction must capture view continuity before destroying the page")
    require(create >= 0 and restore >= 0 and create < restore,
            "page recreation must restore view continuity after construction")
    require("dynamic_cast" not in DISPLAY_CPP,
            "shell continuity must not depend on concrete page RTTI")

    # Production wiring must be explicit, not a hidden global singleton.
    for name, source in (
        ("SynthSequencerPage", SYNTH_H),
        ("PerformPage", PERFORM_H),
        ("FeelPage", FEEL_H),
        ("GenrePage", GENRE_H),
    ):
        require("UiViewContinuityState" in source,
                f"{name} must restore/update shell-owned continuity explicitly")
        require("captureViewContinuity" in source and "restoreViewContinuity" in source,
                f"{name} must implement both continuity directions")

    # Mutation-sensitive editor state is characterized but intentionally not
    # moved in U1D. A later checkpoint needs a validity rule before restoring it.
    require("pattern_edit_cursor_" in PATTERN_H and "has_selection_" in PATTERN_H,
            "Pattern editor residency-sensitive state must remain visible as deferred debt")


if __name__ == "__main__":
    main()
