#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

assert '#include "../phrase_notes_duration_edit.h"' in CPP

start = CPP.index("bool SynthSequencerPage::handlePhraseNotesEvent")
end = CPP.index("void SynthSequencerPage::draw", start)
handler = CPP[start:end]

assert "ui_event.alt" in handler
assert "GROOVEPUTER_LEFT" in handler and "GROOVEPUTER_RIGHT" in handler
assert "PhraseNotesDurationEdit::prepare" in handler
assert "PhraseNotesDurationEdit::commitIfUnchanged" in handler
assert "audio_guard_" in handler
assert "PhraseNotesCursor::tick" in handler
assert "phrase_cursor_.grid" in handler

# Plain navigation remains navigation-only.
assert "PhraseNotesCursor::move" in handler
assert "PhraseNotesCursor::changeGrid" in handler

# U4B3 is duration-only. Note creation/deletion and direct live-field writes are
# intentionally deferred to later checkpoints.
assert "insertSnapped" not in handler
assert "deleteEvent" not in handler
assert "durationSubticks =" not in handler
assert "markSceneMutated" not in handler

# Alt must no longer be rejected before the directional duration gesture can be
# interpreted, while Ctrl/Meta remain outside this local grammar.
assert "ui_event.ctrl || ui_event.alt || ui_event.meta" not in handler

assert '"A+L/R:LEN"' in CPP

print("PASS: U4B3 Phrase duration edit is derived-selection, GRID-sized and guarded")
