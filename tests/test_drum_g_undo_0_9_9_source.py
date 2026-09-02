#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


start = DRUM.find("if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta) {")
require(start >= 0, "plain Drum G handler missing")
end = DRUM.find("// P owns the single P1/P2/P3 request selector", start)
require(end > start, "plain Drum G handler end anchor missing")
g = DRUM[start:end]

# Keep the accepted Stage12 musical generator. Undo integration must wrap it,
# not replace it with the removed Drum-specific quantized-generation path.
require("regenerateDrumsWithStrongRhythmMigration" in g,
        "plain Drum G no longer uses the Stage12 strong-rhythm generator")
require("regenerateDrumsWithQuantizedCommit" not in g and
        "QuantizedGenerationScope::Drums" not in g,
        "Drum G reintroduced a second quantized generation path")

# PREPARE must retain exact before/after fixed pattern values, restore live Scene
# bytes, and only then publish the generated pattern through canonical R9 Pattern
# history. This guarantees one receipt + one revision and makes Ctrl+Z/Ctrl+Z
# use the same DrumPattern exchange path as manual edits.
for token in (
    "DrumPatternUndoPayload before{}",
    "captureCurrentDrumPatternUndo",
    "DrumPatternSet after",
    "sameDrumPattern",
    "undoOwner().commitPrepared",
    "UndoKind::Pattern",
):
    require(token in g, f"Drum G history handoff missing: {token}")

prepare_pos = g.index("regenerateDrumsWithStrongRhythmMigration")
restore_pos = g.index(".patterns[before.patternIndex] = before.before;")
commit_pos = g.index("undoOwner().commitPrepared")
require(prepare_pos < restore_pos < commit_pos,
        "Drum G must restore PREPARE bytes before canonical COMMIT")

# The retained legacy withAudioGuard() is not a pure critical section: it calls
# markSceneMutated() after the callback. Wrapping commitPrepared() with it makes
# the just-published receipt immediately stale (hardware symptom: UNDO: EMPTY).
# Plain G must use the raw AudioGuard for PREPARE and COMMIT apply; the sole
# persistent revision is owned by UndoOwner::commitPrepared().
require("page->withAudioGuard" not in g,
        "Drum G uses legacy withAudioGuard and can double-increment Scene revision")
require("page->audio_guard_(prepareGeneration)" in g,
        "Drum G PREPARE is not protected by the raw AudioGuard")
require("page->audio_guard_(apply)" in g,
        "Drum G COMMIT apply is not protected by the raw AudioGuard")
require("GroovePuterState::markSceneMutated();" not in g,
        "Drum G owns a second persistent revision outside UndoOwner")
raw_guard_pos = g.index("page->audio_guard_(prepareGeneration)")
require(raw_guard_pos < commit_pos,
        "Drum G must finish raw-guard PREPARE before publishing the receipt")

undo_start = DRUM.find("GROOVEPUTER_APP_EVENT_UNDO")
require(undo_start >= 0, "Drum Ctrl+Z owner missing")
undo_end = DRUM.find("// Only the first tab is the DrumSequencerMainPage", undo_start)
undo = DRUM[undo_start:undo_end]
require("togglePrepared<GroovePuterUndo::DrumPatternUndoPayload>" in undo,
        "Drum G receipt has no R9 Undo/Redo consumer")
require("REDO: DRUMS" in undo and "UNDO: DRUMS" in undo,
        "Drum G Undo/Redo lacks visible feedback")

print("0.9.9 Drum G -> Ctrl+Z/Ctrl+Z single-revision history contract: PASS")
