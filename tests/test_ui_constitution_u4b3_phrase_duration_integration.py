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
assert "audio_guard_" in handler
assert "PhraseNotesCursor::tick" in handler
assert "phrase_cursor_.grid" in handler

# U4B3's lasting edit invariant is prepare -> stale/validate -> guarded commit.
# U4B4 strengthens the commit boundary by publishing the before-image to the
# single runtime Phrase Undo owner before the authoritative buffer is changed.
assert "RuntimePhraseEdit::same(live, prepared.before)" in handler
assert "RuntimePhraseEdit::validate(prepared.after)" in handler
assert "commitRuntimePrepared" in handler
assert "UndoKind::RuntimePhrase" in handler
assert "RuntimePhraseEdit::commit(live, prepared.after)" in handler

# Plain navigation remains navigation-only.
assert "PhraseNotesCursor::move" in handler
assert "PhraseNotesCursor::changeGrid" in handler
plain_nav_start = handler.index(
    "if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)"
)
plain_navigation = handler[plain_nav_start:]
assert "RuntimePhraseEdit::commit" not in plain_navigation
assert "commitRuntimePrepared" not in plain_navigation

# U4B3 remains duration-only. Note creation/deletion and direct live-field writes
# are intentionally outside this checkpoint.
assert "insertSnapped" not in handler
assert "deleteEvent" not in handler
assert "durationSubticks =" not in handler
assert "markSceneMutated" not in handler

# Alt must no longer be rejected before the directional duration gesture can be
# interpreted, while Ctrl/Meta remain outside this local grammar.
assert "ui_event.ctrl || ui_event.alt || ui_event.meta" not in handler

assert '"A+L/R:LEN"' in CPP

print("PASS: U4B3 Phrase duration edit remains derived-selection, GRID-sized, stale-guarded and Undo-owned")
