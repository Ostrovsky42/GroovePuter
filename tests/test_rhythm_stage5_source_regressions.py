#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text()
QUANTIZED_WRAPPER = (ROOT / "src/generation/migration/quantized_generation_commit.h").read_text()
QUANTIZED_IMPL = (ROOT / "src/generation/migration/quantized_generation_commit_impl.h").read_text()
QUANTIZED = QUANTIZED_WRAPPER + "\n" + QUANTIZED_IMPL
GENRE_PAGE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()
SCENES_H = (ROOT / "scenes.h").read_text()
SELECTION = (ROOT / "src/generation/composition/rhythm_selection.cpp").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


# Arduino consumes a thin .h wrapper around the implementation header. Source
# ownership checks must inspect the complete quantized owner rather than assume
# every dependency remains textually present in the wrapper itself.
require("quantized_generation_commit_impl.h" in QUANTIZED_WRAPPER,
        "quantized Arduino wrapper no longer exposes its implementation header")

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
variant_block = GENRE_PAGE.split(
    "void GenrePage::cycleRecipeSelection", 1
)[1].split("GenreSettings GenrePage::pendingSettings", 1)[0]
require("GenreCatalog::recipeCount()" not in variant_block,
        "VARIANT selector regressed to the global recipe catalog")
require("recipeIndex_ = static_cast<int>(normalizeRecipeForGenre(" in GENRE_PAGE,
        "changing GENRE must normalize an incompatible VARIANT")

# F-02/F-07 retires MORPH as an active generation axis. Persisted fields stay in
# Scene for decode compatibility, but the Genre surface explicitly normalizes
# them and repeated accepted G owns variation through attemptOrdinal instead.
# UI-P5 deliberately retires the standalone "REROLL"/"REPEAT G" affordance:
# reroll/attempt semantics stay internal to the canonical G generation
# boundary rather than exposing a second UI command for the same operation.
for token in (
    "settings.morphTarget = 0;",
    "settings.morphAmount = 0;",
    "requestedSettings.morphTarget = 0;",
    "requestedSettings.morphAmount = 0;",
    "applyCurrent(true);",
):
    require(token in GENRE_PAGE, f"MORPH retirement contract missing: {token}")
for retired in ("adjustMorph", "morphAccelerator", "morph_amount_", "FocusRow::Morph",
                '"REROLL"', '"REPEAT G"'):
    require(retired not in GENRE_PAGE,
            f"retired MORPH UI owner returned: {retired}")

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

# The generic live bridge preserves rollback ordering: request identity is
# accepted first, complete legacy output is materialized second, then the strong
# Stage15 migration transforms it using that already-assigned ordinal.
allocation_call = BRIDGE.find("assignGenerationAttempt(scene.genre, context")
legacy_call = BRIDGE.find("engine.regeneratePatternsWithGenre();")
migration_call = BRIDGE.find("migrateStrongRhythmMaterial(")
require(allocation_call >= 0, "live Stage 5 bridge no longer accepts reroll request")
require(legacy_call > allocation_call,
        "generation attempt must be allocated before live legacy mutation")
require(migration_call > legacy_call,
        "Stage 5 migration must run after legacy rollback snapshot exists")

# GENRE full generation enters the quantized owner. PLAY and STOP must use the
# same migration core with an ordinal allocated before publication/live mutation.
# STOP intentionally performs the core directly instead of calling the generic
# bridge, which would allocate the same request a second time.
require("quantized_generation_commit.h" in GENRE_PAGE,
        "GenrePage is not wired to quantized material commit")
require("regenerateWithQuantizedCommit(" in GENRE_PAGE,
        "GenrePage MATERIALIZE bypasses quantized commit")
require("allocateAttemptFor(" in QUANTIZED,
        "quantized owner lost accepted-request ordinal allocation")
require("engine.regeneratePatternsWithGenre();" in QUANTIZED,
        "quantized STOP path lost legacy rollback generation")
require("context.generationAttemptOrdinal = attemptOrdinal;" in QUANTIZED,
        "quantized STOP/PLAY path lost assigned attempt identity")
require("migrateStrongRhythmMaterial(" in QUANTIZED,
        "quantized owner no longer applies Stage 5/15 migration")
require("GrooveboxModeManager scratchMode(engine);" in QUANTIZED,
        "quantized PLAY path lost scratch legacy fallback generation")
require("mini_acid_.regeneratePatternsWithGenre();" not in GENRE_PAGE,
        "GenrePage still calls legacy regeneration directly")

prepare = QUANTIZED.split("inline bool preparePlayingCandidate(", 1)[1].split(
    "}  // namespace QuantizedGenerationDetail", 1
)[0]
for required in (
    "RealizationLevel requestLevel",
    "uint32_t generationAttemptOrdinal",
    "context.level = requestLevel;",
    "context.generationAttemptOrdinal = generationAttemptOrdinal;",
):
    require(required in prepare,
            f"playing Stage 5 preparation lost accepted request state: {required}")
for forbidden in (
    "editCurrentSynthPattern(",
    "editCurrentDrumPattern(",
    "engine.setGrooveboxMode(",
    "engine.setBpm(",
    "regenerateWithStrongRhythmMigration(",
):
    require(forbidden not in prepare,
            f"playing Stage 5 preparation mutates live runtime: {forbidden}")

# Stage 7C may persist explicit user rhythm intent. Derived backend, seed, phrase
# coordinates and reroll attempt must still stay out of Scene.
for forbidden in (
    "generationBackend",
    "rhythmBackend",
    "vocabularyBackend",
    "generationSeed",
    "phraseOrdinal",
    "generationAttemptOrdinal",
    "attemptOrdinal",
):
    require(forbidden not in SCENES_H,
            f"Stage 5 must not persist migration state: {forbidden}")

# The Stage 5 materializer still defers semantic synth roles. Later Stage 9 may
# project legacy Synth A pitch through the same migration transaction, while
# the original Dub compatibility path remains explicit and Synth B-only.
for role in (
    "RhythmRole::BassRhythm",
    "RhythmRole::ChordRhythm",
    "RhythmRole::MelodicRhythm",
):
    require(role in MIGRATION, f"Stage 5 does not explicitly defer {role}")
require("candidate.synthA" not in MIGRATION and "candidate.synthB" not in MIGRATION,
        "Stage 5 unexpectedly used fixed-note PatternMaterializer synth binding")
require("return StrongRhythmRoute::DubTechno" in MIGRATION,
        "Dub Techno stab compatibility route is not explicit")
require("return StrongRhythmRoute::DeepChord" in MIGRATION,
        "Deep Chord stab compatibility route is not explicit")
require("result.bassProjectionStatus = projectLegacyPitchPattern(" in MIGRATION,
        "Stage 9 BassRhythm legacy pitch projection is missing")
require("result.chordProjectionStatus = projectLegacyPitchPattern(" in MIGRATION,
        "Stage 10 ChordRhythm legacy pitch projection is missing")
require("realizeChordRhythm" in MIGRATION,
        "Stage 10 ChordRhythm is absent from the shared transaction")
require("realizeMelodicMotif" in MIGRATION and
        "projectLegacyPitchPatternWithOrder" in MIGRATION,
        "Stage 11 Melodic/Motif path is absent from the shared transaction")
require("editCurrentSynthPattern(1)" in BRIDGE,
        "Stage 5 live bridge no longer binds the established Synth B stab slot")
require(BRIDGE.count("editCurrentSynthPattern(0)") == 1,
        "Stage 9 live bridge must bind Synth A exactly once for BassRhythm projection")
require("projectLegacyPitchPattern" in MIGRATION,
        "Stage 9 BassRhythm projection is absent from the shared transaction")

print("Groove Vocabulary Stage 5 source ownership: OK")
