#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
HDR = Path("src/ui/pages/synth_sequencer_page.h").read_text(encoding="utf-8")
CONT = Path("src/ui/ui_view_continuity.h").read_text(encoding="utf-8")

assert '#include "../phrase_notes_viewport.h"' in CPP or '#include "../phrase_notes_viewport.h"' in HDR or 'phrase_notes_viewport.h' in CPP + HDR
assert "PhraseNotesViewport::resolve" in CPP
assert "PhraseNotesCursor::focusBar" in CPP
assert "phraseCursorCell[2]" in CONT
assert "phraseGrid[2]" in CONT
assert "phraseFocusBar[2]" not in CONT
assert "phrase_focus_bar_" not in HDR

old = "std::min<uint16_t>(\n      phrase.lengthTicks, 2 * PhraseRuntime::kTicksPerBar)"
assert old not in CPP, "Phrase draw must not stay hard-wired to the first two bars"

# The window is still derived rather than hard-wired -- the assertion above
# guarantees that -- but it is no longer expressed in subticks. The single-lane
# renderer resolves a bar window and a magnified detail window, both in ticks,
# so the names to pin are those. windowStartSubtick/windowEndSubtick belonged
# to the retired lane-packed renderer.
assert "barStart" in CPP and "barEnd" in CPP, \
    "the drawn bar window must still be derived from the viewport"
# The magnified lane is gone: pitch on the vertical axis shows a short note and
# the melody's shape at once, so a second view of the same bar is no longer
# needed. The law it served -- every drawn window is derived, never fixed --
# now applies to the pitch axis instead.
assert "lowestNote" in CPP and "centreNote" in CPP, \
    "the pitch window must be derived from the selection, not fixed"
assert "anchorNote" in CPP, \
    "the pitch window must follow the selected sound"

assert "UI::drawStandardFooter" in CPP or "UI::publishShellFooter" in CPP
# Footer literals moved to plain words with the beginner redesign. Pinned as
# the key plus the words for its effect, so further wording work does not
# require editing this gate again.
assert "L/R PICK" in CPP, "the cursor binding must stay advertised"
assert "HIGHER LOWER" in CPP, "the pitch binding must stay advertised"
assert "U/D:GRID" not in CPP, \
    "Up/Down is bound to pitch now; advertising it as GRID would be false"

handler_start = CPP.index("bool SynthSequencerPage::handlePhraseNotesEvent")
handler_end = CPP.index("void SynthSequencerPage::draw", handler_start)
handler = CPP[handler_start:handler_end]
assert "GROOVEPUTER_LEFT" in handler and "GROOVEPUTER_RIGHT" in handler
assert "PhraseNotesCursor::move" in handler
assert "PhraseNotesViewport::moveFocus" not in handler
assert "RuntimePhraseEdit::commit" not in handler
assert "commitPreparedPhrase" not in handler

print("PASS: U4A Phrase viewport remains bounded and now follows the GRID cursor")
