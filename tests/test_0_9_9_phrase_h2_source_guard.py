#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FROZEN_W1 = "34912cd050c04727c13533575b2cf999816e0549"
PRODUCTION = ROOT / "src/generation/composition/phrase_harmonic_clock_projection.h"
PROTECTED = [
    "src/generation/roles/harmonic_rhythm.h",
    "src/generation/migration/strong_rhythm_migration.h",
    "src/generation/migration/strong_rhythm_migration.cpp",
    "src/generation/roles/chord_progression.h",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/composition/phrase_harmonic_timeline.h",
]

text = PRODUCTION.read_text(encoding="utf-8")

required = [
    "realizeHarmonicRhythm(request)",
    "makePhraseHarmonicTimeline",
    "phraseHarmonicEventRangeForBar",
    "request.phraseBarOrdinal = bar",
    "request.phraseHarmonicPosition = nextPhraseOrdinal",
    "production execution\n// wiring is deferred to PHRASE-P1R",
]
for token in required:
    assert token in text, f"missing H2 production contract token: {token!r}"

forbidden = [
    "chord.plan.onsets",
    "ChordRhythm",
    "patternAddress",
    "Song",
    "transport",
    "MIDI",
    "QuarterCycle",
    "0,4,8,12",
    "0,6,10",
    "0,12",
    "genre",
    "BPM",
    "std::vector",
    "new ",
    "malloc",
    "free(",
    "realizeChordProgression",
    "Melodic",
    "lifetime",
    "runtime publication",
]
for token in forbidden:
    assert token not in text, f"forbidden H2 production dependency/policy: {token!r}"

for path in PROTECTED:
    current = (ROOT / path).read_bytes()
    frozen = subprocess.check_output(
        ["git", "show", f"{FROZEN_W1}:{path}"], cwd=ROOT
    )
    assert current == frozen, f"frozen W1/H1/C1 owner changed in H2: {path}"

assert "F08.1" not in text, "F08.1 must not be imported into H2 production"
print("H2 source guard: OK")
print("W1/H1/C1 protected owners unchanged from frozen W1: YES")
print("F08.1 imported: NO")
