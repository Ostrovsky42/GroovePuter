from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/chord_rhythm.h").read_text()
SOURCE = (ROOT / "src/generation/roles/chord_rhythm.cpp").read_text()
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
SCENES = (ROOT / "scenes.h").read_text()

for name in (
    "HeldPad", "WholeBarHold", "HalfBarChange", "OffbeatStab",
    "BackbeatStab", "AnticipatedChange", "SparseChordReply",
    "DubChordSpace", "SyncopatedComp",
):
    assert name in HEADER, f"missing ChordRhythm identity: {name}"

assert "GenerationDomain::ChordRhythmSelection" in SOURCE
for forbidden in ("ChordPitch", "GenreSettings", "Scene", "SynthPattern", "rand(", "new "):
    assert forbidden not in SOURCE, f"ChordRhythm leaked forbidden owner: {forbidden}"
assert "realizeChordRhythm" in MIGRATION
assert "RhythmRole::ChordRhythm" in MIGRATION
assert "projectLegacyPitchPattern" in MIGRATION
assert "chordRhythmId" not in SCENES
