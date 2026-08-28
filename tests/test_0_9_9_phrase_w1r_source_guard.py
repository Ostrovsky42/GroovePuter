#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
H1_F1 = "eae498dc5b6377ddc4a45c2e62a7c33afab92e6c"
OLD_W1 = "34912cd050c04727c13533575b2cf999816e0549"

chord_h = "src/generation/roles/chord_progression.h"
chord_cpp = "src/generation/roles/chord_progression.cpp"
w1_production = {
    "src/generation/roles/harmonic_rhythm.h",
    "src/generation/migration/strong_rhythm_migration.h",
    "src/generation/migration/strong_rhythm_migration.cpp",
}

subprocess.run(
    ["git", "diff", "--exit-code", H1_F1, "--", chord_h, chord_cpp],
    cwd=ROOT,
    check=True,
)

changed_src = subprocess.check_output(
    ["git", "diff", "--name-only", H1_F1 + "...HEAD", "--", "src/"],
    cwd=ROOT,
    text=True,
).splitlines()
assert set(changed_src) == w1_production, (
    f"W1R production delta differs from frozen W1 owner set: {changed_src}"
)

for path in sorted(w1_production):
    subprocess.run(
        ["git", "diff", "--exit-code", OLD_W1, "--", path],
        cwd=ROOT,
        check=True,
    )

harmonic = (ROOT / "src/generation/roles/harmonic_rhythm.h").read_text()
strong_h = (ROOT / "src/generation/migration/strong_rhythm_migration.h").read_text()
strong_cpp = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
compat = (ROOT / "tests/test_0_9_9_phrase_w1r_h1_f1_compat.cpp").read_text()
production_text = "\n".join((harmonic, strong_h, strong_cpp))

for forbidden in (
    "ChordProgressionSource",
    "realizeChordProgressionSource",
    "chordProgressionEventAt",
    "chordProgressionSourceEventAt",
    "phraseGlobalHarmonicOrdinal",
    "PhraseHarmonicTimeline",
    "phrase_harmonic_clock_projection",
):
    assert forbidden not in production_text, f"W1R crossed H1-F1/H2 boundary: {forbidden}"

for forbidden in (
    "chordProgressionEventAt(",
    "chordProgressionSourceEventAt(",
):
    assert forbidden not in compat, f"W1R compatibility consumed arbitrary accessor: {forbidden}"

assert not re.search(
    r"progressionRequest\s*\.\s*harmonicEventCount\s*=\s*"
    r"onsetCount\s*\(\s*chord\s*\.\s*plan\s*\.\s*onsets\s*\)",
    strong_cpp,
), "ChordRhythm regained progression event-cardinality ownership"
assert re.search(
    r"progressionRequest\s*\.\s*harmonicEventCount\s*=\s*"
    r"harmonic\s*\.\s*plan\s*\.\s*eventCount",
    strong_cpp,
), "HarmonicRhythm no longer owns progression finite-plan cardinality"

assert "return isStaticHarmonicProgression(id) ? 1 : 2;" in harmonic
assert "evenlySpacedHarmonicOnsets" in harmonic
for forbidden in (
    "0,4,8,12", "0,6,10", "0,12",
):
    assert forbidden not in harmonic, f"forbidden harmonic clock vocabulary: {forbidden}"

changed = subprocess.check_output(
    ["git", "diff", "--name-only", H1_F1 + "...HEAD"],
    cwd=ROOT,
    text=True,
).splitlines()
for path in changed:
    lower = path.lower()
    if path.startswith("src/"):
        assert path in w1_production, f"unexpected production owner: {path}"
    assert "phrase_harmonic_clock_projection" not in lower, path
    assert "p1r" not in lower, path
    assert "c2" not in lower, path
    assert "r1" not in lower, path
    assert "i2" not in lower, path
    assert "song" not in lower, path
    assert "transport" not in lower, path
    assert "midi" not in lower, path
    assert "internal_synth" not in lower, path

diff = subprocess.check_output(
    ["git", "diff", H1_F1 + "...HEAD", "--", *sorted(w1_production)],
    cwd=ROOT,
    text=True,
)
added = "\n".join(
    line[1:] for line in diff.splitlines()
    if line.startswith("+") and not line.startswith("+++")
)
for token in ("new ", "malloc(", "calloc(", "realloc("):
    assert token not in added, f"heap use introduced in W1R: {token}"

print("W1R source guard: H1-F1 chord_progression byte identity=YES")
print("W1R source guard: old W1 production replay byte identity=YES")
print("W1R source guard: production owner set=EXACT")
print("W1R source guard: arbitrary source accessor consumption=ABSENT")
print("W1R source guard: progression event-count source=HarmonicRhythm")
print("W1R source guard: F08.1 imported=NO")
print("W1R source guard: downstream owners=ABSENT")
print("W1R source guard: heap delta=ABSENT")
