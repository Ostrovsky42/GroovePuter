#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
HEADER = ROOT / "src/generation/migration/strong_rhythm_migration.h"
HARMONIC = ROOT / "src/generation/roles/harmonic_rhythm.h"
QUARANTINE = ROOT / "tests/test_0_9_9_f08_bootstrap_quarantine.cpp"

migration = MIGRATION.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
harmonic = HARMONIC.read_text(encoding="utf-8")
quarantine = QUARANTINE.read_text(encoding="utf-8")

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

# Bootstrap debt must stay explicit without making Genre or raw BPM the owner.
assert "F08 BOOTSTRAP QUARANTINE" in quarantine
assert "kFutureMinimumDistinctMovingClocks = 4" in quarantine
for progression in (
    "ProgressionId::PopCycle",
    "ProgressionId::TwoFiveOne",
    "ProgressionId::ParallelShift",
    "ProgressionId::MinorFall",
    "ProgressionId::BorrowedLift",
):
    assert progression in quarantine
for forbidden in (
    "GenreSettings",
    "GenerativeMode",
    "GenreId",
    "ProgressionId::StaticModal",
    "ProgressionId::PedalDrone",
    "bpm",
    "BPM",
):
    assert forbidden not in quarantine

# Quarantine semantics are intentionally inverted: today's collapsed vocabulary
# is an expected XFAIL with green CI, while future success must become an XPASS
# failure so the quarantine cannot silently fossilize into a permanent contract.
xpass_start = quarantine.index(
    "if (distinctCount >= kFutureMinimumDistinctMovingClocks)"
)
xpass_end = quarantine.index("EXPECTED XFAIL", xpass_start)
xpass_branch = quarantine[xpass_start:xpass_end]
assert "XPASS:" in xpass_branch
assert "return 1;" in xpass_branch
xfail_branch = quarantine[xpass_end:]
assert "EXPECTED XFAIL:" in xfail_branch
assert "return 0;" in xfail_branch

print("0.9.9-F08 source ownership + bootstrap-quarantine regressions: OK")
