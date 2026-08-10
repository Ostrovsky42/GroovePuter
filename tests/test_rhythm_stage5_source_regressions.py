#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text()
GENRE_PAGE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()
SCENES_H = (ROOT / "scenes.h").read_text()
SELECTION = (ROOT / "src/generation/composition/rhythm_selection.cpp").read_text()


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

# Hardware correction: the GENRE row must constrain which VARIANT recipes are
# selectable. A global recipe list allowed e.g. Techno + Chicago Jack, where the
# recipe route completely masked the selected base genre.
for token in (
    "kAcidRecipes",
    "kRaveRecipes",
    "kDubRecipes",
    "kBreakRecipes",
    "recipeChoicesForGenre",
    "normalizeRecipeForGenre",
):
    require(token in GENRE_PAGE,
            f"genre-scoped VARIANT contract missing: {token}")
require("GenreCatalog::recipeCount()" not in GENRE_PAGE.split(
            "void GenrePage::cycleRecipeSelection", 1)[1].split(
            "void GenrePage::adjustMorph", 1)[0],
        "VARIANT selector regressed to the global recipe catalog")
require("recipeIndex_ = static_cast<int>(normalizeRecipeForGenre(" in GENRE_PAGE,
        "changing GENRE must normalize an incompatible VARIANT")

# Hardware correction: Chicago Jack and Rolling Acid were audibly too similar.
# Keep their curated Stage 5 archetype pools disjoint. Deep Chord must use the
# chord-response grammar instead of behaving like another generic Dub route.
for token in (
    "constexpr RhythmCompatibilityCandidate kChicagoJack[]",
    "candidate(Archetype::StraightAcid, 120)",
    "candidate(Archetype::SparseAcid, 100)",
    "constexpr RhythmCompatibilityCandidate kRollingAcid[]",
    "candidate(Archetype::RollingAcid, 120)",
    "candidate(Archetype::SyncopatedAcid, 110)",
    "constexpr RhythmCompatibilityCandidate kDeepChord[]",
    "candidate(Archetype::ChordResponse, 120)",
):
    require(token in SELECTION,
            f"hardware identity correction missing: {token}")

# Runtime bridge must preserve rollback ordering: complete legacy output is
# materialized first, then the transactional Vocabulary material migration.
legacy_call = BRIDGE.find("engine.regeneratePatternsWithGenre();")
migration_call = BRIDGE.find("migrateStrongRhythmMaterial(")
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

# Stage 7C may persist explicit user rhythm intent. Derived backend, seed and
# phrase coordinates must still stay out of Scene.
for forbidden in (
    "generationBackend",
    "rhythmBackend",
    "vocabularyBackend",
    "generationSeed",
    "phraseOrdinal",
):
    require(forbidden not in SCENES_H,
            f"Stage 5 must not persist migration state: {forbidden}")

# Bass and melodic semantics remain deferred. ChordRhythm is also ignored by
# the generic PatternMaterializer; the only synth compatibility path is legacy
# Synth B event relocation for Dub Techno / Deep Chord.
for role in (
    "RhythmRole::BassRhythm",
    "RhythmRole::ChordRhythm",
    "RhythmRole::MelodicRhythm",
):
    require(role in MIGRATION, f"Stage 5 does not explicitly defer {role}")
require("candidate.synthA" not in MIGRATION and "candidate.synthB" not in MIGRATION,
        "Stage 5 unexpectedly used fixed-note PatternMaterializer synth binding")
require("route == StrongRhythmRoute::DubTechno" in MIGRATION,
        "Dub Techno stab compatibility route is not explicit")
require("route == StrongRhythmRoute::DeepChord" in MIGRATION,
        "Deep Chord stab compatibility route is not explicit")
require("remapLegacyStab" in MIGRATION,
        "Stage 5 lacks the legacy-pitch stab compatibility adapter")
require("sourceEvents[sourceCount++] = legacy.steps[step]" in MIGRATION,
        "Dub adapter no longer sources complete legacy Synth B events")
require("editCurrentSynthPattern(1)" in BRIDGE,
        "Stage 5 live bridge no longer binds the established Synth B stab slot")
require("editCurrentSynthPattern(0)" not in BRIDGE,
        "Stage 5 must not bind Synth A/bass pitch before Bass Generator v2")

print("Groove Vocabulary Stage 5 source ownership: OK")
