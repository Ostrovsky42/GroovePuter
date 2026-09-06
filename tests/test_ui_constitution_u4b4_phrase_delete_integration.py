#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

assert '#include "../phrase_notes_delete_edit.h"' in CPP

start = CPP.index("bool SynthSequencerPage::handlePhraseNotesEvent")
end = CPP.index("void SynthSequencerPage::draw", start)
handler = CPP[start:end]

assert "isBackspace" in handler
assert "PhraseNotesDeleteEdit::prepare" in handler
assert "PhraseNotesDeleteEdit::commitIfUnchanged" in handler
assert "PhraseNotesCursor::tick" in handler
assert "audio_guard_" in handler

# Plain Backspace owns delete; Alt+Backspace is not consumed by this slice.
assert "isBackspace && !ui_event.alt" in handler

# Existing cursor/grid/duration grammar must remain intact.
assert "PhraseNotesCursor::move" in handler
assert "PhraseNotesCursor::changeGrid" in handler
assert "PhraseNotesDurationEdit::prepare" in handler
assert "PhraseNotesDurationEdit::commitIfUnchanged" in handler

# U4B4 is deletion-only. Insertion, pitch entry and Undo stay separate.
assert "insertSnapped" not in handler
assert "noteForEntryKey" not in handler
assert "undoOwner" not in handler
assert "markSceneMutated" not in handler

assert '"BS:DEL A+L/R:LEN"' in CPP

print("PASS: U4B4 Phrase delete is derived-selection and guarded")
