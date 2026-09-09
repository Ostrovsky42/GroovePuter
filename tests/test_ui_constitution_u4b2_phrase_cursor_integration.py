#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src/ui/pages/synth_sequencer_page.h").read_text(encoding="utf-8")
CONT = (ROOT / "src/ui/ui_view_continuity.h").read_text(encoding="utf-8")
SELECTION = (ROOT / "src/ui/phrase_selection_state.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require('phrase_notes_cursor.h' in CPP + HDR,
            "U4B2 Phrase NOTES must use the GRID cursor owner")
    require('phrase_notes_selection.h' in CPP + HDR,
            "U4B2 Phrase NOTES must retain the live-Phrase selection projection")
    require('phrase_selection_state.h' in CPP + HDR,
            "U4B2 Phrase NOTES must use the shared stable selection owner")
    require('PhraseNotesCursor::tick' in CPP,
            "U4B2 renderer must derive the visible cursor tick from GRID state")
    require('PhraseNotesCursor::focusBar' in CPP,
            "U4B2 viewport focus must follow cursor time")

    # U4D1 strengthened the original U4B2 selection model. Cursor coverage is
    # still used to pick a sound under the time cursor, but the selected sound
    # itself must survive reorder/delete/undo and switching between roll/list.
    # Requiring PhraseNotesSelection::derive() here would force the UI back to
    # the old ephemeral "whatever is under this tick" selection semantics.
    require('PhraseSelectionState::resolve' in CPP,
            "U4B2 selected musical object must be re-resolved from stable identity")
    require('PhraseSelectionState::first' in CPP,
            "U4B2 must recover a lawful selection when the current sound disappears")
    require('PhraseSelectionState::State phrase_selection_' in HDR,
            "U4B2 roll/list views must share one selected-sound state")
    require('startTick' in SELECTION and 'note' in SELECTION,
            "stable selection identity must describe the sound, not only its array slot")
    require('PhraseNotesSelection::deriveInCell' in CPP,
            "cursor coverage must remain the adapter for choosing a sound under the GRID cell")

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
    require('phrase_pitch_lowest_' in CPP,
            "the pitch window must have explicit, inspectable position state")
    require('phrase_pitch_lowest_' not in CONT,
            "the pitch window is view state and must not be persisted")
    require('phrase_pitch_lowest_ = 0' not in handler,
            "picking a sound must not reset the pitch window")
    require('U/D:GRID' not in CPP,
            "Up/Down is bound to pitch now; advertising it as GRID would lie")
    require('selected' in CPP.lower() and 'drawRect' in CPP,
            "U4B2 renderer must visibly distinguish the derived selected span")


if __name__ == '__main__':
    main()
