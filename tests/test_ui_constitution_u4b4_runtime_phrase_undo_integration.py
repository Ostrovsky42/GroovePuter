#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SLOT = (ROOT / "src/state/bounded_undo_slot.h").read_text(encoding="utf-8")
OWNER = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
RECEIPTS = (ROOT / "src/state/undo_receipts.h").read_text(encoding="utf-8")
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

assert "RuntimePhrase" in SLOT, (
    "runtime Phrase needs a distinct UndoKind; persistent PhraseBank Undo cannot be reused"
)
assert "RuntimePhraseUndoPayload" in RECEIPTS, (
    "runtime Phrase needs a fixed session-only before-image receipt"
)
assert "RuntimeSynthEventBuffer" in RECEIPTS, (
    "runtime Phrase receipt must retain the authoritative bounded runtime buffer"
)
assert "commitRuntimePrepared" in OWNER, (
    "single UndoOwner needs a runtime-only commit path that does not mark Scene dirty"
)
assert "toggleRuntimePrepared" in OWNER, (
    "single UndoOwner needs a runtime-only undo/redo toggle path"
)

start = SYNTH.index("bool SynthSequencerPage::handlePhraseNotesEvent")
end = SYNTH.index("void SynthSequencerPage::draw", start)
phrase_handler = SYNTH[start:end]
assert "commitRuntimePrepared" in phrase_handler, (
    "Phrase duration edit must publish its before-image to the single Undo owner"
)

handle_start = SYNTH.index("bool SynthSequencerPage::handleEvent")
handle_end = SYNTH.index("const std::string& SynthSequencerPage::getTitle", handle_start)
handle = SYNTH[handle_start:handle_end]
assert "UndoKind::RuntimePhrase" in handle, (
    "Synth NOTES must recognize the runtime Phrase receipt"
)
assert "toggleRuntimePrepared" in handle, (
    "Ctrl+Z must restore/toggle runtime Phrase through the single Undo owner"
)
assert "UNDO: PHRASE" in handle and "REDO: PHRASE" in handle, (
    "runtime Phrase Undo/Redo must remain visible to the musician"
)

# A duration receipt belongs to the Phrase NOTES editing context. Switching the
# synth back to Pattern must not let Ctrl+Z silently navigate the source back to
# Phrase; the global Undo layer should retain the receipt and report NOT HERE.
assert "if (phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event)" in handle, (
    "runtime Phrase Undo must be gated by the authoritative Phrase NOTES context"
)
assert "if (!phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event)" in handle, (
    "Pattern Undo must remain independently gated by Pattern NOTES context"
)

# U4B4 is undo ownership only. Do not smuggle insertion/deletion into this slice.
assert "insertSnapped" not in phrase_handler
assert "deleteEvent" not in phrase_handler

print("PASS: U4B4 runtime Phrase Undo has one owner, no Scene-dirty alias, and no source-navigation side effect")
