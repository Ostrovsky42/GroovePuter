#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)

ux = read("src/ui/undo_ux.h")
owner = read("src/state/undo_owner.h")
slot = read("src/state/bounded_undo_slot.h")
receipts = read("src/state/undo_receipts.h")
display = read("src/ui/miniacid_display.cpp")
synth = read("src/ui/pages/synth_sequencer_page.cpp")
pattern_legacy = read("src/ui/pages/pattern_edit_page_legacy.h")
drum = read("src/ui/pages/drum_sequencer_page.cpp")
drum_legacy = read("src/ui/pages/drum_sequencer_page_legacy.h")
song = read("src/ui/pages/song_page_r4_owner.inc")
phrase = read("src/ui/pages/phrase_page.cpp")
genre = read("src/ui/pages/genre_page.cpp")
generation = read("src/generation/migration/quantized_generation_undo_owner_impl.h")
commit_impl = read("src/generation/migration/quantized_generation_commit_impl.h")
tb = read("src/ui/pages/tb303_params_page.cpp")
build = read("scripts/build.sh")
atlas = read("src/dsp/atlas_runtime.h")

# The hardware log proved Z and U reach the input layer distinctly. Only Z is
# public history. U must never silently remain as an alias.
require("GROOVEPUTER_Z" in ux and "key == 26" in ux,
        "Ctrl+Z public promotion was not restored")
require("GROOVEPUTER_U" not in ux and "key == 21" not in ux,
        "Ctrl+U regression remains wired as Undo")
require("promoteUndoShortcut(event);" in display,
        "global history promotion disappeared")

# One retained pair, not an Undo stack + Redo stack.
for token in ("togglePrepared", "exchangeFixedValue", "nextIsRedo"):
    require(token in owner, f"R9 owner primitive missing: {token}")
require("next_is_redo_" in slot, "R9 direction bit missing")
for token in ("DrumPatternUndoPayload", "exchangeSynthPatternUndo",
              "exchangeDrumPatternUndo", "exchangeSongUndo",
              "exchangePhraseUndo"):
    require(token in receipts, f"R9 receipt/exchange missing: {token}")

# Visible Synth NOTES owns Pattern history; hidden KNOBS/MORE do not steal it.
require("GroovePuterUndoUx::isUndoEvent(ui_event)" in synth,
        "Synth parent lost visible-owner Ctrl+Z routing")
require("synth_tab_ == SynthTab::Notes" in synth,
        "Synth history is not constrained to visible NOTES")
require("REDO: PATTERN" in synth and "UNDO: PATTERN" in synth,
        "Synth one-slot feedback is not bidirectional")
require("togglePrepared<SynthPatternUndoPayload>" in pattern_legacy,
        "Pattern child still performs destructive one-way Undo")

# R9 restores history ownership for MANUAL Drum Pattern mutations only. Plain
# Drum G is an existing 0.9.9 Stage12/strong-rhythm generation contract and is
# deliberately outside this regression fix; do not silently quantize/rewrite it.
require("GROOVEPUTER_APP_EVENT_UNDO" in drum,
        "Drum page lost public history owner")
require("REDO: DRUMS" in drum and "UNDO: DRUMS" in drum,
        "Drum manual one-slot feedback missing")
require("commitDrumPatternMutation" in drum_legacy,
        "manual Drum edits bypass canonical UndoOwner")
require("regenerateDrumsWithStrongRhythmMigration" in drum,
        "plain Drum G no longer preserves the D3/Stage12 generation path")
require("regenerateDrumsWithQuantizedCommit" not in drum,
        "Undo regression fix must not create a second Drum generation path")
require("QuantizedGenerationScope::Drums" not in generation and
        "Drums," not in commit_impl,
        "Undo regression fix leaked a Drum-specific quantized generation scope")

# D3-specific domains keep their newer lifecycle while regaining one-slot Redo.
require("const bool redo = owner.nextIsRedo();" in song and
        "togglePrepared<SongUndoPayload>" in song,
        "Song D3 lost R9 one-slot history")
require("cancelPendingSongActivationForRevision" in song,
        "Song Undo no longer invalidates matching pending activation")
require(phrase.count("const bool redo = owner.nextIsRedo();") >= 2,
        "Phrase/Song ordinary receipts lost Redo")
require("toggleLastQuantizedGeneration" in generation,
        "Generation STOP-state Redo owner missing")
require("matchingPending" in generation and "ContextUnavailable" in generation,
        "Generation Redo lacks PLAY/pending safety")
require("REDO: GEN" in genre and "UNDO: GEN" in genre,
        "Genre history owner missing")

# Ctrl+Z collision closure on TB303 remains part of the contract.
for scan in ("GROOVEPUTER_A", "GROOVEPUTER_X", "GROOVEPUTER_C", "GROOVEPUTER_V"):
    require(scan in tb, f"TB303 reset input closure missing: {scan}")

# Carry the independently verified generation-reset fix without pulling the
# hardware-validation documentation branch into production ancestry.
require('ARDUINO_LOOP_STACK_SIZE="${ARDUINO_LOOP_STACK_SIZE:-32768}"' in build,
        "3bba2437 LoopTask stack fix missing")
require("GROOVEPUTER_DSP_ATLAS_RUNTIME_H" in atlas,
        "3bba2437 atlas_runtime include guard missing")

print("0.9.9 R8/R9 Undo regression contracts: PASS")
