#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "74456bcfec0fc74138ec0d8c652dde642c7e16b6"

harmonic = (ROOT / "src/generation/roles/harmonic_rhythm.h").read_text()
strong_h = (ROOT / "src/generation/migration/strong_rhythm_migration.h").read_text()
strong_cpp = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()

# Exact accepted owner boundary: no physical/runtime policy inputs.
request_match = re.search(
    r"struct\s+HarmonicRhythmRequest\s*\{(?P<body>.*?)\};",
    harmonic,
    re.S,
)
assert request_match, "HarmonicRhythmRequest missing"
request = request_match.group("body")
for forbidden in (
    "ChordRhythmId", "ChordRhythmPlan", "ChordRhythm", "StepMask",
    "patternAddress", "Song", "transport", "genre", "Genre", "BPM", "bpm",
):
    assert forbidden not in request, f"forbidden HarmonicRhythm request owner: {forbidden}"

# F08.1 firewall: W1 production may only keep the accepted static/moving bootstrap.
for forbidden in (
    "0,4,8,12", "0,6,10", "0,12", "PopCycle ->", "TwoFiveOne ->",
    "ParallelShift ->", "MinorFall ->", "BorrowedLift ->",
):
    assert forbidden not in harmonic, f"F08.1 vocabulary leaked into W1: {forbidden}"

assert "return isStaticHarmonicProgression(id) ? 1 : 2;" in harmonic
assert "evenlySpacedHarmonicOnsets" in harmonic

# The old ownership coupling must be gone from production.
assert not re.search(
    r"progressionRequest\s*\.\s*harmonicEventCount\s*=\s*"
    r"onsetCount\s*\(\s*chord\s*\.\s*plan\s*\.\s*onsets\s*\)",
    strong_cpp,
), "ChordRhythm still owns progression event cardinality"
assert re.search(
    r"progressionRequest\s*\.\s*harmonicEventCount\s*=\s*"
    r"harmonic\s*\.\s*plan\s*\.\s*eventCount",
    strong_cpp,
), "ChordProgression cardinality is not sourced from HarmonicRhythm"

# TonalMaterializer must receive independent harmonic timing, while physical role
# articulation remains a separate argument.
assert "request.harmonicEventOnsets = harmonicEventOnsets;" in strong_cpp
assert strong_cpp.count("harmonic.plan.onsets") >= 5, "tonal roles not wired to harmonic WHEN"
assert "harmonicEventOnsets" in strong_h
assert "harmonicEventCount" in strong_h
assert "HarmonicRhythmStatus" in strong_h

# W1 production delta is narrowly bounded. Phrase projection is deliberately absent
# under Decision B; no Song/UI/storage/lifetime owner may appear.
changed = subprocess.check_output(
    ["git", "diff", "--name-only", BASE + "...HEAD"],
    cwd=ROOT,
    text=True,
).splitlines()
production = [p for p in changed if p.startswith("src/")]
allowed = {
    "src/generation/roles/harmonic_rhythm.h",
    "src/generation/migration/strong_rhythm_migration.h",
    "src/generation/migration/strong_rhythm_migration.cpp",
}
assert set(production) <= allowed, f"unexpected W1 production delta: {production}"
for path in changed:
    assert not path.startswith("src/ui/"), f"UI change forbidden in W1: {path}"
    assert "song" not in path.lower(), f"Song/storage publication forbidden in W1: {path}"

# No W1 production phrase projection was guessed.
assert "PhraseHarmonicTimeline" not in strong_cpp
assert "eventPositionsByBar" not in strong_cpp

print("W1 source guard: F08 owner recovered=YES")
print("W1 source guard: ChordRhythm independence=YES")
print("W1 source guard: progression event-count source=HarmonicRhythm")
print("W1 source guard: TonalMaterializer WHEN source=HarmonicRhythm")
print("W1 source guard: F08.1 imported=NO")
print("W1 source guard: phrase projection implementation=ABSENT_DECISION_B")
print("W1 source guard: production files=" + ",".join(production))
