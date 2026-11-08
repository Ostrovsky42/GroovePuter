#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

assert '#include "../phrase_notes_delete_edit.h"' in CPP

start = CPP.index("bool SynthSequencerPage::handlePhraseNotesEvent")
end = CPP.index("void SynthSequencerPage::draw", start)
handler = CPP[start:end]

assert "isBackspace" in handler
assert "PhraseNotesDeleteEdit::prepare" in handler
assert "commitRuntimePhraseEditWithUndo" in handler
assert "PhraseNotesDeleteEdit::commitIfUnchanged" not in handler
assert "PhraseNotesCursor::tick" in handler
assert "audio_guard_" in handler

# Plain Backspace owns delete; Alt+Backspace is not consumed by this slice.
assert "isBackspace && !ui_event.alt" in handler

# Existing cursor/grid/duration grammar must remain intact. U4B5 may route both
# successful mutation gestures through the single Runtime Phrase Undo boundary.
assert "PhraseNotesCursor::move" in handler
assert "PhraseNotesCursor::changeGrid" in handler
assert "PhraseNotesDurationEdit::prepare" in handler
assert "PhraseNotesDurationEdit::commitIfUnchanged" not in handler

# The U4B4 *gesture* remains deletion-only. The handler is no longer
# insertion-free -- U4B8 deliberately added Enter, so the blanket ban on
# insertion here is spent and pretending otherwise would let this gate pass on
# a technicality (the adapter is called, not the primitive). What still holds:
# the delete branch itself must not create anything, insertion must go through
# its own policy adapter rather than the raw primitive, and nothing here may
# write live state or dirty the Scene.
backspace_branch = handler[handler.index("isBackspace &&"):]
backspace_branch = backspace_branch[:backspace_branch.index("if (ui_event.alt)")]
assert "insertSnapped" not in backspace_branch
assert "PhraseNotesInsertEdit" not in backspace_branch

assert "insertSnapped" not in handler, \
    "insertion must go through PhraseNotesInsertEdit, not the raw primitive"
assert "PhraseNotesInsertEdit::prepare" in handler, \
    "U4B8 insertion must be reachable from this handler"
assert "noteForEntryKey" not in handler
assert "markSceneMutated" not in handler

# Same requirement as before, new literal: delete must stay permanently
# advertised in the footer alongside the other Phrase commands.
assert "BS DEL" in CPP

print("PASS: U4B4 Phrase delete is derived-selection, guarded and Runtime-Undo owned")
