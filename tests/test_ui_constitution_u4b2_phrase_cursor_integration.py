#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src/ui/pages/synth_sequencer_page.h").read_text(encoding="utf-8")
CONT = (ROOT / "src/ui/ui_view_continuity.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require('phrase_notes_cursor.h' in CPP + HDR,
            "U4B2 Phrase NOTES must use the GRID cursor owner")
    require('phrase_notes_selection.h' in CPP + HDR,
            "U4B2 Phrase NOTES must derive selection from the live Phrase")
    require('PhraseNotesCursor::tick' in CPP,
            "U4B2 renderer must derive the visible cursor tick from GRID state")
    require('PhraseNotesCursor::focusBar' in CPP,
            "U4B2 viewport focus must follow cursor time")
    require('PhraseNotesSelection::derive' in CPP,
            "U4B2 selected musical object must be derived from cursor coverage")
    require('PhraseNotesViewport::moveFocus' not in CPP,
            "U4B2 must retire independent coarse bar-focus navigation")
    require('phrase_focus_bar_' not in HDR,
            "U4B2 must not keep a second independent Phrase focus state")

    require('phraseCursorCell[2]' in CONT and 'phraseGrid[2]' in CONT,
            "U4B2 runtime continuity must retain GRID cursor state per synth")
    require('phraseFocusBar[2]' not in CONT,
            "U4B2 focus must be derived from cursor, not separately persisted")
    require('selectedEvent' not in CONT and 'eventIndex' not in CONT,
            "U4B2 must never persist a buffer event index as selection identity")

    handler_start = CPP.index('bool SynthSequencerPage::handlePhraseNotesEvent')
    handler_end = CPP.index('void SynthSequencerPage::draw', handler_start)
    handler = CPP[handler_start:handler_end]
    for key in ('GROOVEPUTER_LEFT', 'GROOVEPUTER_RIGHT',
                'GROOVEPUTER_UP', 'GROOVEPUTER_DOWN'):
        require(key in handler, f"U4B2 handler missing spatial/grid navigation: {key}")
    require('PhraseNotesCursor::move' in handler,
            "plain Left/Right must move the cursor by one current GRID cell")
    # Grid/zoom moved to ALT+Up/Down when plain Up/Down was reassigned to
    # pitch. The owner must still be the only thing that changes the grid.
    require('PhraseNotesCursor::changeGrid' in handler,
            "the GRID/zoom change must still go through the cursor owner")
    require('PhraseNotesPitchEdit::prepare' in handler,
            "plain Up/Down must edit pitch through the U4B7 policy adapter")
    require('RuntimePhraseEdit::commit' not in handler and
            'commitPreparedPhrase' not in handler,
            "U4B2 navigation must remain mutation-free")

    require('gridLabel' in CPP,
            "U4B2 must make the active GRID observable")
    # The footer grammar changed with the beginner redesign: abbreviations
    # like 'U/D:GRID' were replaced by plain words, and Up/Down no longer means
    # grid at all. The requirement itself is unchanged -- every binding the
    # screen offers must be permanently visible in the footer.
    require('L/R PICK' in CPP,
            "U4B2 footer must name the cursor binding in plain words")
    require('HIGHER LOWER' in CPP,
            "U4B2 footer must name the pitch binding in plain words")
    require('SHORTER' in CPP and 'LONGER' in CPP,
            "U4B2 footer must name the length binding in plain words")
    require('UNDO' in CPP,
            "U4B2 must keep undo permanently visible")
    # The second lane is gone; the pitch axis replaced it. What must hold in
    # its place is stronger, and is the reason the axis is safe to add:
    # browsing the pitch range is a separate gesture from editing a pitch, and
    # the browsing offset is view state that never becomes musical state.
    # This gate demanded that picking a sound reset the window, and that was
    # wrong: it made the whole picture rearrange on every press, with notes
    # leaving the screen and others moving. The window must hold still while
    # the selection is inside it and scroll only far enough to bring it back
    # when it leaves an edge -- which is what these pin instead.
    require('phrase_pitch_lowest_' in CPP,
            "the pitch window must have explicit, inspectable position state")
    require('phrase_pitch_lowest_' not in CONT,
            "the pitch window is view state and must not be persisted")
    # Anchored to the handler rather than to a line of code: the first version
    # of this pinned a literal expression, which a refactor removed and broke
    # the gate for no product reason.
    require('phrase_pitch_lowest_ = 0' not in handler,
            "picking a sound must not reset the pitch window")
    require('U/D:GRID' not in CPP,
            "Up/Down is bound to pitch now; advertising it as GRID would lie")
    require('selected' in CPP.lower() and 'drawRect' in CPP,
            "U4B2 renderer must visibly distinguish the derived selected span")


if __name__ == '__main__':
    main()
