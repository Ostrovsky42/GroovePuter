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
    require('PhraseNotesCursor::changeGrid' in handler,
            "plain Up/Down must change only the cursor GRID/zoom")

    # U4B2 owns the lasting navigation law, not a permanent ban on later edit
    # commands. U4B3+ may add explicit guarded mutations before these branches;
    # plain L/R and U/D must remain navigation-only.
    plain_nav_start = handler.index(
        'if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)'
    )
    plain_navigation = handler[plain_nav_start:]
    require('RuntimePhraseEdit::commit' not in plain_navigation and
            'commitPreparedPhrase' not in plain_navigation and
            'commitRuntimePrepared' not in plain_navigation,
            "U4B2 plain cursor/grid navigation must remain mutation-free")

    require('gridLabel' in CPP,
            "U4B2 must make the active GRID observable")
    require('L/R:CUR' in CPP and 'U/D:GRID' in CPP,
            "U4B2 footer must expose the new navigation grammar")
    require('selected' in CPP.lower() and 'drawRect' in CPP,
            "U4B2 renderer must visibly distinguish the derived selected span")


if __name__ == '__main__':
    main()
