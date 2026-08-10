from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASS_H = (ROOT / "src/generation/roles/bass_rhythm.h").read_text()
BASS_CPP = (ROOT / "src/generation/roles/bass_rhythm.cpp").read_text()
PROJECTOR = (ROOT / "src/generation/roles/semantic_pattern_projector.cpp").read_text()
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
SCENES = (ROOT / "scenes.h").read_text()

for name in (
    "RootPulse", "KickLock", "KickAnswer", "GapFill", "OffbeatPush",
    "SparseAnchor", "RollingDrive", "HalfTimePocket", "SyncopatedHook",
    "SustainAndDrop",
):
    assert name in BASS_H, f"missing BassRhythm identity: {name}"

assert "GenerationDomain::BassRhythmSelection" in BASS_CPP
assert "BassPitch" not in BASS_CPP
assert "SynthPattern" not in BASS_CPP
assert "GenreSettings" not in BASS_CPP
assert "Scene" not in BASS_CPP
assert "rand(" not in BASS_CPP
assert "new " not in BASS_CPP
assert "projectLegacyPitchPattern" in MIGRATION
assert "editCurrentSynthPattern(0)" not in MIGRATION
assert "engineName" not in PROJECTOR
assert "rhythmArchetypeId" in SCENES
assert "bassRhythmId" not in SCENES
