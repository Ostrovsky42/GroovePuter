#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
HEADER = ROOT / "src/generation/migration/strong_rhythm_migration.h"
HARMONIC = ROOT / "src/generation/roles/harmonic_rhythm.h"
VOCABULARY_CORPUS = ROOT / "tests/test_0_9_9_f08_1_vocabulary_corpus.cpp"

migration = MIGRATION.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
harmonic = HARMONIC.read_text(encoding="utf-8")
vocabulary_corpus = VOCABULARY_CORPUS.read_text(encoding="utf-8")

assert "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" not in migration
assert "harmonic.plan.onsets" in migration
assert "progressionRequest.harmonicEventCount = harmonic.plan.eventCount;" in migration
assert '"../roles/harmonic_rhythm.h"' in header

# HarmonicRhythm must not regain physical ChordRhythm ownership.
for forbidden in ("ChordRhythmPlan", "ChordRhythmId", "chordOnsets"):
    assert forbidden not in harmonic
for forbidden in ("GenreSettings", "GenerativeMode", "GenreId", "bpm", "BPM"):
    assert forbidden not in harmonic

# The F08.1 policy is a small named progression vocabulary, not a count/genre
# table invented to satisfy the old >=4 debt detector.
for clock in (
    "HarmonicClockId::StaticHold",
    "HarmonicClockId::HalfBarPivot",
    "HarmonicClockId::QuarterCycle",
    "HarmonicClockId::CadentialThree",
    "HarmonicClockId::LateChange",
):
    assert clock in harmonic
for progression in (
    "ProgressionId::StaticModal",
    "ProgressionId::PedalDrone",
    "ProgressionId::PopCycle",
    "ProgressionId::TwoFiveOne",
    "ProgressionId::ParallelShift",
    "ProgressionId::MinorFall",
    "ProgressionId::BorrowedLift",
):
    assert progression in harmonic

assert "stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12)" in harmonic
assert "stepBit(0) | stepBit(6) | stepBit(10)" in harmonic
assert "stepBit(0) | stepBit(12)" in harmonic

# E3a coordinates remain API coordinates here; F08.1 does not add a scheduler,
# transport owner, lifecycle owner, or Song-state dependency.
assert "phraseBarOrdinal" in harmonic
assert "phraseHarmonicPosition" in harmonic
for forbidden in ("Song", "transport", "playback", "lifecycle"):
    assert forbidden not in harmonic.replace(
        "scheduler or phrase-position policy", ""
    )

# The full migration still obtains HarmonicRhythm solely from progression and
# gives its event count to ChordProgression. No chord-plan field feeds the clock.
harmonic_request_start = migration.index("HarmonicRhythmRequest harmonicRequest{};")
harmonic_request_end = migration.index("ChordProgressionRequest progressionRequest{};", harmonic_request_start)
harmonic_request_block = migration[harmonic_request_start:harmonic_request_end]
assert "harmonicRequest.progression = result.progressionId;" in harmonic_request_block
assert "chord.plan" not in harmonic_request_block
assert "chordOnsets" not in harmonic_request_block

# The deterministic review corpus pins literal project seeds and phrase ordinals.
for seed in (
    "0x13579bdfu",
    "0x2468ace0u",
    "0x31415926u",
    "0x27182818u",
    "0x0badf00du",
    "0xc001d00du",
    "0x5eed1234u",
):
    assert seed in vocabulary_corpus
assert "generation.projectSeed" in vocabulary_corpus
assert "generation.phraseOrdinal" in vocabulary_corpus

print("0.9.9-F08.1 source ownership + harmonic-vocabulary regressions: OK")
