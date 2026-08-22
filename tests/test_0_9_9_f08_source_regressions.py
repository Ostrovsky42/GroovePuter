#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
HEADER = ROOT / "src/generation/migration/strong_rhythm_migration.h"
HARMONIC = ROOT / "src/generation/roles/harmonic_rhythm.h"

migration = MIGRATION.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
harmonic = HARMONIC.read_text(encoding="utf-8")

assert "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" not in migration
assert "harmonic.plan.onsets" in migration
assert "progressionRequest.harmonicEventCount = harmonic.plan.eventCount;" in migration
assert '"../roles/harmonic_rhythm.h"' in header

# F08 must not make ChordRhythm an input to the new harmonic clock.
assert "ChordRhythmPlan" not in harmonic
assert "ChordRhythmId" not in harmonic
assert "chordOnsets" not in harmonic

# E3a coordinates are API-only here: no scheduler/transport/phrase storage owner.
assert "phraseBarOrdinal" in harmonic
assert "phraseHarmonicPosition" in harmonic
for forbidden in ("Song", "scheduler", "transport", "playback", "lifecycle"):
    assert forbidden not in harmonic.replace(
        "scheduler, lifecycle, or cross-bar progression state", ""
    )

print("0.9.9-F08 source ownership regressions: OK")
