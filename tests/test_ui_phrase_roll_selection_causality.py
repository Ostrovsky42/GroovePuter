#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

start = SYNTH.index("if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {")
end = SYNTH.index("// Switch view.", start)
block = SYNTH[start:end]

if "PhraseNotesCursor::move" not in block:
    raise AssertionError("roll navigation stopped moving the insertion cursor")
if "PhraseNotesSelection::deriveInCell" not in block:
    raise AssertionError(
        "roll cursor can enter a sound cell without resolving the sound under it"
    )
if "PhraseSelectionState::at" not in block:
    raise AssertionError(
        "roll cursor can resolve a sound but does not promote it to shared selection"
    )
if "PhraseSelectionState::withInsertTick" not in block:
    raise AssertionError(
        "roll navigation does not preserve the independent insertion position"
    )

print("Phrase roll selection causality: PASS")
