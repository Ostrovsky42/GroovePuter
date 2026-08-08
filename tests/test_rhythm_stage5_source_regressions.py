#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text()
GENRE_PAGE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()
SCENES_H = (ROOT / "scenes.h").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


# The allow-list must remain explicit. In particular, routing by GrooveboxMode
# would accidentally absorb Reggae/TripHop/UK Garage/Footwork before Stage 12.
require("selectStrongRhythmRoute" in MIGRATION, "missing explicit Stage 5 route selector")
for recipe in ("case 2:", "case 5:", "case 6:", "case 7:", "case 10:"):
    require(recipe in MIGRATION, f"missing approved Stage 5 recipe {recipe}")
for mode in (
    "GenerativeMode::Acid",
    "GenerativeMode::Darksynth",
    "GenerativeMode::Rave",
):
    require(mode in MIGRATION, f"missing approved Stage 5 base mode {mode}")
require("GrooveboxMode" not in MIGRATION,
        "Stage 5 selector must not route broad GrooveboxMode families")

# Runtime bridge must preserve the rollback ordering: complete legacy output is
# materialized first, then the transactional drum migration is attempted.
legacy_call = BRIDGE.find("engine.regeneratePatternsWithGenre();")
migration_call = BRIDGE.find("migrateStrongRhythmDrums(")
require(legacy_call >= 0, "Stage 5 bridge no longer calls legacy generator")
require(migration_call > legacy_call,
        "Stage 5 migration must run after legacy rollback snapshot exists")

# Only the explicit GENRE materialization boundary opts into Stage 5 in this PR.
require("strong_rhythm_live_bridge.h" in GENRE_PAGE,
        "GenrePage is not wired to Stage 5 bridge")
require("regenerateWithStrongRhythmMigration(mini_acid_)" in GENRE_PAGE,
        "GenrePage MATERIALIZE bypasses Stage 5 bridge")
require("mini_acid_.regeneratePatternsWithGenre();" not in GENRE_PAGE,
        "GenrePage still calls legacy regeneration directly")

# No persisted migration/backend ownership may be added to Scene in Stage 5.
for forbidden in (
    "generationBackend",
    "rhythmBackend",
    "vocabularyBackend",
    "archetypeId",
    "generationSeed",
    "phraseOrdinal",
):
    require(forbidden not in SCENES_H,
            f"Stage 5 must not persist migration state: {forbidden}")

# Stage 5 is drum-only at this boundary. VoiceRole/Bass/Phrase ownership belongs
# to later stages, so migration must explicitly defer all synth rhythm roles.
for role in (
    "RhythmRole::BassRhythm",
    "RhythmRole::ChordRhythm",
    "RhythmRole::MelodicRhythm",
):
    require(role in MIGRATION, f"Stage 5 does not explicitly defer {role}")
require("candidate.synthA" not in MIGRATION and "candidate.synthB" not in MIGRATION,
        "Stage 5 unexpectedly started physical synth-role migration")

print("Groove Vocabulary Stage 5 source ownership: OK")
