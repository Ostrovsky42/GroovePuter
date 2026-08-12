#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/generation/roles/chord_rhythm_timeline.h"
CPP = ROOT / "src/generation/roles/chord_rhythm_timeline.cpp"


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
    "retrigger",
):
    assert forbidden not in source, f"P2 timeline leaked foreign owner token: {forbidden}"

header = HEADER.read_text()
for required in (
    "using ChordRhythmTimelineMask = uint64_t;",
    "kMaxChordRhythmTimelineBars = kMaxPhraseBars",
    "ChordRhythmTimelineMask onsets",
    "ChordRhythmTimelineMask continuations",
    "ChordRhythmTimelineMask releasePoints",
    "InvalidTopology",
):
    assert required in header, f"P2 missing contract token: {required}"

# Pin the actual Stage15 gate surface that motivated the P2 correction: release
# points are a first-class existing ChordRhythm semantic and may not be inferred
# away from onset positions.
legacy_header = (ROOT / "src/generation/roles/chord_rhythm.h").read_text()
assert "StepMask onsets" in legacy_header
assert "StepMask continuations" in legacy_header
assert "StepMask releasePoints" in legacy_header

migration = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
assert "realizeChordRhythmTimeline" not in migration
assert "chord_rhythm_timeline" not in migration

print("P2 multi-bar ChordRhythm source regressions: OK")
