from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/melodic_motif.h").read_text()
SOURCE = (ROOT / "src/generation/roles/melodic_motif.cpp").read_text()
PROJECTOR = (ROOT / "src/generation/roles/semantic_pattern_projector.cpp").read_text()
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
SCENES = (ROOT / "scenes.h").read_text()

for name in (
    "SparseCall", "DelayedAnswer", "TwoNoteHook", "PickupPhrase",
    "LongTone", "RestHeavy", "BarEndResponse", "SyncopatedMotif",
    "DriftPhrase", "RepeatedCell",
):
    assert name in HEADER, f"missing MelodicRhythm identity: {name}"

for name in ("SourceOrder", "TwoNoteCell", "Mirror", "CallResponse", "Pivot"):
    assert name in HEADER, f"missing MotifShape identity: {name}"

assert "GenerationDomain::MelodicRhythmSelection" in SOURCE
assert "GenerationDomain::MotifSelection" in SOURCE
for forbidden in ("LeadPitch", "GenreSettings", "Scene", "SynthPattern", "rand(", "new "):
    assert forbidden not in SOURCE, f"Melodic/Motif leaked forbidden owner: {forbidden}"
assert "projectLegacyPitchPatternWithOrder" in PROJECTOR
assert "SemanticSynthBRole" in MIGRATION
assert "RhythmRole::MelodicRhythm" in MIGRATION
assert "melodicRhythmId" not in SCENES and "motifShapeId" not in SCENES
