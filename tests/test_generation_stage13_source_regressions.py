from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/composition/generation_profile.h").read_text()
SOURCE = (ROOT / "src/generation/composition/generation_profile.cpp").read_text()
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
SCENES = (ROOT / "scenes.h").read_text()

for edge in (
    "rhythms", "feels", "bassRhythms", "chordRhythms",
    "melodicRhythms", "motifShapes", "phraseLaws",
):
    assert edge in HEADER, f"missing composition edge: {edge}"

assert "rhythmCompatibilityFor(settings)" in SOURCE
assert "GenerationDomain::BassRhythmSelection" in SOURCE
assert "GenerationDomain::ChordRhythmSelection" in SOURCE
assert "GenerationDomain::MelodicRhythmSelection" in SOURCE
assert "GenerationDomain::MotifSelection" in SOURCE
assert "GenerationDomain::FeelProfileSelection" in SOURCE
assert "GenerationDomain::PhraseLawSelection" in SOURCE
assert "requestedId = result.bassRhythmId" in MIGRATION
assert "requestedId = result.chordRhythmId" in MIGRATION
assert "requestedRhythm = result.melodicRhythmId" in MIGRATION
assert "requestedShape = result.motifShapeId" in MIGRATION
assert "context.feelProfile" in MIGRATION
assert "suggestedFeel" not in SCENES
assert "phraseLaw" not in SCENES
assert "evolveMultiBarPhrase" not in MIGRATION

for forbidden in (
    "stepBit(", "SynthPattern", "NoteEvent", "materializeRhythmPattern",
    "Mood", "TextureMode", "new ", "rand(", "malloc(",
):
    assert forbidden not in SOURCE, f"composition matrix leaked owner: {forbidden}"
