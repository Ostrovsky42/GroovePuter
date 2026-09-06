#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require('#include "../phrase_notes_lane_layout.h"' in CPP,
            "U4B1 renderer must use the temporal lane layout owner")
    require("PhraseNotesLaneLayout::build(" in CPP,
            "U4B1 renderer must build lanes from musical time spans")
    require("laneByEvent" in CPP,
            "U4B1 renderer must map each event through the temporal layout")
    require("i % maxRows" not in CPP,
            "U4B1 must remove buffer-index-derived vertical position")
    require("kOverflowLane" in CPP,
            "U4B1 renderer must handle visual lane overflow explicitly")


if __name__ == "__main__":
    main()
