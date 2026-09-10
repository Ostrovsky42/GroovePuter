#!/usr/bin/env python3
"""U4B1 integration contract for the Phrase NOTES renderer.

History, so the change of assertions is auditable rather than convenient:
this file used to require PhraseNotesLaneLayout::build, laneByEvent and
kOverflowLane -- the multi-row packing where a note's row depended on which
other notes were present. That packing was retired by an explicit product
decision (docs/design/PHRASE_SCREEN_FOR_A_FIRST_TIME_USER.md): the screen now
draws one horizontal lane in which vertical position carries no meaning at all.

The invariant those assertions actually protected -- vertical geometry must
never be derived from buffer order -- is kept and strengthened below: there is
no vertical geometry to derive. What replaces them pins the new claim with the
same strictness: horizontal position comes from musical time, width comes from
the projected duration span, and the stored-but-silent part of a note is drawn
as something visibly different from the part that sounds.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("PhraseNotesProjection::project(" in CPP,
            "U4B1 renderer must take each note's extent from the projection owner")
    require("tickToX" in CPP,
            "U4B1 horizontal position must be derived from musical time")
    require("audibleEnd" in CPP,
            "U4B1 renderer must resolve where the next attack truncates a note")
    require("drawMutedTail(" in CPP,
            "U4B1 must render stored-but-silent duration distinctly from sounding "
            "duration; hiding it would make ALT+LEFT/RIGHT edit an invisible value")

    require("PhraseNotesLaneLayout::build(" not in CPP,
            "U4B1 single lane must not reintroduce neighbour-dependent row packing")
    require("i % maxRows" not in CPP,
            "U4B1 must never derive vertical position from buffer index")


if __name__ == "__main__":
    main()
