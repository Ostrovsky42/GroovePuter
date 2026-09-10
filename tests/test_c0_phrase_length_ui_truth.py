#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")

needle = "PhraseInstrumentControls::applyLengthChangeDetailed("
if needle not in CPP:
    raise AssertionError("Synth Phrase LENGTH must preserve the detailed domain outcome")

if '"NOTES BEYOND %uB"' not in CPP:
    raise AssertionError("blocked Phrase shrink must explain that notes exist beyond the target")

if 'changed ? toast : "LENGTH UNCHANGED"' in CPP:
    raise AssertionError("generic LENGTH UNCHANGED must not hide safe-shrink refusal")

print("C0 H1 Synth Phrase LENGTH surface truth: PASS")
