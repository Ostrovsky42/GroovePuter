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

# Stage 12 multi-bar evolution is now intentionally reachable only through the
# explicit Cardputer audition/probe owner. Normal full/drums production G routes
# stay one-bar and must not call the phrase-evolution API directly.
assert '#include "../phrase/phrase_evolution.h"' in BRIDGE
assert "PhraseAuditionResult regeneratePhraseAuditionWithProbe" in BRIDGE
assert "phrase = evolveMultiBarPhrase(request);" in BRIDGE
assert "runSubtractiveRuntimeProbe" in BRIDGE

full_route = BRIDGE.split(
    "StrongRhythmMigrationResult regenerateWithStrongRhythmMigration", 1
)[1].split(
    "StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration", 1
)[0]
drum_route = BRIDGE.split(
    "StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration", 1
)[1].split("const char* phraseAuditionStatusName", 1)[0]
assert "evolveMultiBarPhrase" not in full_route
assert "evolveMultiBarPhrase" not in drum_route

assert "request.phraseBars = 1;" in MIGRATION
assert "evolveMultiBarPhrase" not in MIGRATION
