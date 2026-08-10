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
assert "phrase_evolution" not in BRIDGE
assert "evolveMultiBarPhrase" not in BRIDGE
assert "evolveMultiBarPhrase" not in MIGRATION
