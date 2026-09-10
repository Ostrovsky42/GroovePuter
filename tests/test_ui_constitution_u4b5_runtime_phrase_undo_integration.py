#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SLOT = (ROOT / "src/state/bounded_undo_slot.h").read_text(encoding="utf-8")
OWNER = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
RECEIPTS = (ROOT / "src/state/undo_receipts.h").read_text(encoding="utf-8")
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

assert "RuntimePhrase" in SLOT
assert "RuntimePhraseUndoPayload" in RECEIPTS
assert "RuntimeSynthEventBuffer" in RECEIPTS
assert "commitRuntimePrepared" in OWNER
assert "toggleRuntimePrepared" in OWNER
assert "markSceneMutated" not in OWNER[OWNER.index("commitRuntimePrepared"):OWNER.index("private:")]

phrase_start = SYNTH.index("bool SynthSequencerPage::handlePhraseNotesEvent")
phrase_end = SYNTH.index("void SynthSequencerPage::draw", phrase_start)
phrase_handler = SYNTH[phrase_start:phrase_end]
assert "PhraseNotesDurationEdit::prepare" in phrase_handler
assert "PhraseNotesDeleteEdit::prepare" in phrase_handler
assert phrase_handler.count("commitRuntimePhraseEditWithUndo") >= 2, (
    "duration and delete must publish through one runtime Phrase commit boundary"
)
assert "markSceneMutated" not in phrase_handler

handle_start = SYNTH.index("bool SynthSequencerPage::handleEvent")
handle_end = SYNTH.index("const std::string& SynthSequencerPage::getTitle", handle_start)
handle = SYNTH[handle_start:handle_end]
assert "if (phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event)" in handle, (
    "runtime Phrase Undo must be gated by authoritative Phrase NOTES context"
)
assert "UndoKind::RuntimePhrase" in handle
assert "toggleRuntimePrepared" in handle
assert "UNDO: PHRASE" in handle and "REDO: PHRASE" in handle
assert "if (!phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event)" in handle, (
    "Pattern Undo must remain independently gated by Pattern NOTES context"
)

print("PASS: U4B5 single-owner runtime Phrase Undo is session-only and context-gated")
