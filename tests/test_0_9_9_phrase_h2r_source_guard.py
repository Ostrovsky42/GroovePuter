#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
W1R = "329dcb91e40feb734182f437a8a50f2b61b40fd2"
OLD_H2 = "c9c0dc852dfb96b191c5d7066c81af99e3df189a"
PRODUCTION_PATH = "src/generation/composition/phrase_harmonic_clock_projection.h"
PRODUCTION = ROOT / PRODUCTION_PATH

PROTECTED = [
    "src/generation/roles/chord_progression.h",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/roles/harmonic_rhythm.h",
    "src/generation/migration/strong_rhythm_migration.h",
    "src/generation/migration/strong_rhythm_migration.cpp",
    "src/generation/composition/phrase_harmonic_timeline.h",
    "src/generation/migration/phrase_semantic_result.h",
]

text = PRODUCTION.read_text(encoding="utf-8")
old_h2 = subprocess.check_output(["git", "show", f"{OLD_H2}:{PRODUCTION_PATH}"], cwd=ROOT)
assert PRODUCTION.read_bytes() == old_h2, "H2R production projection is not byte-identical to frozen old H2"

for path in PROTECTED:
    current = (ROOT / path).read_bytes()
    frozen = subprocess.check_output(["git", "show", f"{W1R}:{path}"], cwd=ROOT)
    assert current == frozen, f"protected H1-F1/W1R owner changed in H2R: {path}"

required = [
    "realizeHarmonicRhythm(request)",
    "request.phraseBarOrdinal = bar",
    "request.phraseHarmonicPosition = nextPhraseOrdinal",
    "makePhraseHarmonicTimeline",
    "phraseHarmonicEventRangeForBar",
]
for token in required:
    assert token in text, f"missing frozen H2 projection token: {token!r}"

forbidden = [
    "ChordRhythm",
    "chord.plan.onsets",
    "ChordProgressionSource",
    "realizeChordProgressionSource",
    "chordProgressionSourceEventAt",
    "chordProgressionEventAt",
    "realizeChordProgression(",
    "Song",
    "transport",
    "MIDI",
    "synth",
    "patternAddress",
    "storage",
    "QuarterCycle",
    "F08.1",
    "genre",
    "BPM",
    "feel",
    "std::vector",
    "std::string",
    "new ",
    "malloc",
    "free(",
    "C2",
    "R1",
    "I2",
]
for token in forbidden:
    assert token not in text, f"forbidden H2R production owner/policy: {token!r}"

changed = subprocess.check_output(
    ["git", "diff", "--name-only", f"{W1R}...HEAD"], cwd=ROOT, text=True
).splitlines()
allowed = {
    PRODUCTION_PATH,
    "tests/test_0_9_9_phrase_h2r_harmonic_clock_projection.cpp",
    "tests/test_0_9_9_phrase_h2r_source_guard.py",
    "tests/run_0_9_9_phrase_h2r_tests.sh",
    ".github/workflows/0-9-9-phrase-h2r-h1-f1-replay.yml",
    "docs/contracts/0_9_9_PHRASE_H2R_H1_F1_REPLAY.md",
}
extra = sorted(set(changed) - allowed)
assert not extra, f"unexpected H2R delta outside replay scope: {extra}"

print("H2R source guard: OK")
print("old H2 production projection byte-identical: YES")
print("H1-F1/W1R protected owners unchanged: YES")
print("H2 production consumes ChordProgressionSource: NO")
print("H2 production consumes chordProgressionEventAt: NO")
print("F08.1 / QuarterCycle / downstream execution imported: NO")
