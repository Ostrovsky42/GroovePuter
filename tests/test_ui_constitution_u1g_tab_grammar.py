#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
PERFORM = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # Field-list grammar: Up/Down already own focus movement. Plain Tab must not
    # duplicate that action because Tab is reserved for peer/local representation.
    require("UIInput::isTab(event)" not in FEEL,
            "FEEL must not consume plain Tab as next-field navigation")
    require("TAB/U/D:FIELD" not in FEEL,
            "FEEL footer must not advertise Tab as field navigation")
    require("U/D:FIELD L/R:CHANGE" in FEEL,
            "FEEL must keep explicit Up/Down field navigation help")
    require("nav == GROOVEPUTER_UP" in FEEL and "nav == GROOVEPUTER_DOWN" in FEEL,
            "FEEL Up/Down focus navigation must remain intact")

    require("UIInput::isTab(event)" not in GENRE,
            "GENRE must not consume plain Tab as next-field navigation")
    require("TAB/U/D:FIELD" not in GENRE,
            "GENRE footer must not advertise Tab as field navigation")
    require("U/D:FIELD L/R:CHANGE" in GENRE,
            "GENRE must keep explicit Up/Down field navigation help")
    require("nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN" in GENRE,
            "GENRE Up/Down focus navigation must remain intact")

    # Peer/local representation consumers are preservation targets.
    require("UIInput::isTab(ui_event)" in SYNTH and
            "setSynthTab(static_cast<SynthTab>(next))" in SYNTH,
            "Synth Tab must keep cycling NOTES/KNOBS/MORE")
    require("const bool tabPressed" in PERFORM and "moveContext(" in PERFORM,
            "PERFORM Tab must keep owning its local KEY/CHORD/ARP/RHYTHM layer")

    # The global workflow chord stays a higher-priority modified Tab command.
    require("event.meta && (event.key == '\\t' || event.scancode == GROOVEPUTER_TAB)" in DISPLAY,
            "Meta/Fn+Tab workflow switching must remain globally owned")
    page_dispatch = DISPLAY.index("if (currentPage->handleEvent(event))")
    workflow_tab = DISPLAY.index("event.meta && (event.key == '\\t' || event.scancode == GROOVEPUTER_TAB)")
    require(workflow_tab < page_dispatch,
            "Fn+Tab workflow switching must stay ahead of page-local dispatch")


if __name__ == "__main__":
    main()
