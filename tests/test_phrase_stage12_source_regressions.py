from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/phrase/phrase_evolution.h").read_text()
SOURCE = (ROOT / "src/generation/phrase/phrase_evolution.cpp").read_text()
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text()
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()

assert "evolveRhythmPhrase(" in SOURCE
assert "phraseBars == 8 ? 4" in SOURCE
assert "&first.identity" in SOURCE
assert "kMaxProductionPhraseBars = 8" in HEADER
for forbidden in ("Scene", "Song", "PhraseCore", "page", "bank", "rand(", "new "):
    assert forbidden not in SOURCE, f"Stage 12 orchestration leaked owner: {forbidden}"

# Normal production remains one-bar. Only the explicit Cardputer audition/probe
# entry point may reach the Stage 12 candidate before the physical gate passes.
assert "request.phraseBars = 1;" in MIGRATION
assert "evolveMultiBarPhrase" not in MIGRATION
assert "phraseEvolutionCatalog" not in MIGRATION

whole = BRIDGE.split(
    "StrongRhythmMigrationResult regenerateWithStrongRhythmMigration", 1
)[1].split(
    "StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration", 1
)[0]
drums = BRIDGE.split(
    "StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration", 1
)[1].split(
    "const char* phraseAuditionStatusName", 1
)[0]
for name, section in (("whole G", whole), ("DRUMS G", drums)):
    assert "evolveMultiBarPhrase" not in section, f"{name} leaked Stage 12 caller"
    assert "phraseEvolutionCatalog" not in section, f"{name} leaked Stage 12 catalog"

audition = BRIDGE.split(
    "PhraseAuditionResult regeneratePhraseAuditionWithProbe", 1
)[1]
assert "ReferenceVocabulary::phraseEvolutionCatalog()" in audition
assert "evolveMultiBarPhrase(request)" in audition
