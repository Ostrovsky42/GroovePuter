#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/generation/roles/chord_rhythm_retrigger.h"
CPP = ROOT / "src/generation/roles/chord_rhythm_retrigger.cpp"


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


source = strip_comments(HEADER.read_text() + "\n" + CPP.read_text())
for forbidden in (
    "HarmonicEvent",
    "ChordProgression",
    "ChordQuality",
    "kMaxHarmonicEvents",
    "GenreProfile",
    "SynthPattern",
    "Scene",
    "sendNoteOn",
    "sendNoteOff",
    "ChordRhythmTimeline",
):
    assert forbidden not in source, f"P3 leaked foreign owner token: {forbidden}"

header = HEADER.read_text()
for required in (
    "sourceAdvanceOnsets",
    "sameChordRetriggers",
    "audibleOnsets",
    "continuations",
    "releasePoints",
    "sourceOrdinalByStep",
    "sourceAvailableAtStart",
    "kIncomingChordRhythmSourceOrdinal",
    "InvalidGateTopology",
):
    assert required in header, f"P3 missing contract token: {required}"

# Cross-bar composition must be an explicit caller input, never a hidden lookup
# into Scene/migration/global harmonic state.
assert "sourceAvailableAtStart" in source
assert "kIncomingChordRhythmSourceOrdinal" in source

legacy_header = (ROOT / "src/generation/roles/chord_rhythm.h").read_text()
assert "StepMask continuations" in legacy_header
assert "StepMask releasePoints" in legacy_header

migration = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
assert "realizeChordRhythmRetriggers" not in migration
assert "chord_rhythm_retrigger" not in migration

print("P3 same-chord retrigger source regressions: OK")
