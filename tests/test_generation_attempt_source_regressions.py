#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text(encoding="utf-8")
STATE = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text(encoding="utf-8")
QUANTIZED = (ROOT / "src/generation/migration/quantized_generation_commit_impl.h").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
SCENES = (ROOT / "scenes.h").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# F-02: MORPH remains decode-compatible Scene data but is no longer a route or
# RNG input. Preserve the exact two historical hash positions as literal zeros so
# a default morph=0 scene with attempt=0 stays bit-identical to the frozen corpus.
seed = MIGRATION.split("uint32_t projectSeedFor(", 1)[1].split(
    "uint32_t realizationSeedFor(", 1
)[0]
for needle in (
    "mixByte(hash, settings.generativeMode)",
    "mixByte(hash, settings.recipe)",
    "mixByte(hash, 0)",
    "mixByte(hash, static_cast<uint8_t>(route))",
):
    require(seed, needle, f"legacy-compatible attempt-0 seed changed: {needle}")
if seed.count("mixByte(hash, 0)") != 2:
    raise AssertionError("attempt-0 seed must preserve exactly two former MORPH zero slots")
for retired in ("settings.morphTarget", "settings.morphAmount"):
    if retired in seed:
        raise AssertionError(f"persisted MORPH returned to project seed: {retired}")

selector = MIGRATION.split("StrongRhythmRoute selectStrongRhythmRoute", 1)[1].split(
    "StrongRhythmMigrationResult migrateStrongRhythmDrums", 1
)[0]
for retired in ("morphTarget", "morphAmount"):
    if retired in selector:
        raise AssertionError(f"persisted MORPH returned to strong route selection: {retired}")

# GA-02: ordinal zero is a literal compatibility bypass. Any refactor that mixes
# even a zero ordinal would silently refreeze every deterministic corpus.
realization_seed = MIGRATION.split("uint32_t realizationSeedFor(", 1)[1].split(
    "bool validLevel", 1
)[0]
require(realization_seed, "if (attemptOrdinal == 0) return selectionSeed;",
        "attempt zero lost exact legacy seed bypass")
require(realization_seed, "kGenerationAttemptSalt",
        "non-zero reroll seed lost explicit domain salt")

# GA-07: composition/archetype identity is selected from selectionSeed only;
# attempt variation begins only at the rhythm realization and role realization
# layer. Do not allow attemptOrdinal into composition selection.
drums = MIGRATION.split("StrongRhythmMigrationResult migrateStrongRhythmDrums", 1)[1].split(
    "namespace {", 1
)[0]
for needle in (
    "const uint32_t selectionSeed = projectSeedFor(settings, result.route);",
    "const uint32_t realizationSeed =",
    "realizationSeedFor(selectionSeed, context.generationAttemptOrdinal);",
    "selectionGeneration.projectSeed = selectionSeed;",
    "resolveGenerationComposition(settings, selectionGeneration);",
    "request.generation.projectSeed = realizationSeed;",
):
    require(drums, needle, f"selection/realization seed split changed: {needle}")

# GA-01/03/05/06: fixed session-only owner, exact tuple axes, no eviction,
# persistence or heap. Capacity exhaustion must fail closed.
for needle in (
    "kGenerationAttemptCapacity = 64",
    "generativeMode",
    "recipe",
    "RealizationLevel level",
    "patternAddress",
    "GenerationAttemptStatus::TableFull",
    "return {GenerationAttemptStatus::TableFull, 0};",
):
    require(STATE, needle, f"attempt owner contract changed: {needle}")
for forbidden in (
    "Preferences", "NVS", "std::vector", "std::map", "unordered_map",
    "malloc(", "calloc(", "new ",
):
    if forbidden in STATE:
        raise AssertionError(f"attempt owner gained persistence/heap dependency: {forbidden}")
for forbidden in ("generationAttemptOrdinal", "attemptOrdinal"):
    if forbidden in SCENES:
        raise AssertionError(f"attempt identity leaked into Scene persistence: {forbidden}")

# GA-03/04: both live and quantized request paths assign before mutation and pass
# the exact assigned ordinal forward. Publication serial is telemetry only.
for needle in (
    "assignGenerationAttempt(scene.genre, context",
    "context.generationAttemptOrdinal = allocation.ordinal;",
):
    require(BRIDGE, needle, f"live accepted-attempt path changed: {needle}")
for needle in (
    "allocateAttemptFor(requestedGenre, requestLevel, target, attemptOrdinal)",
    "context.generationAttemptOrdinal = attemptOrdinal;",
    "context.generationAttemptOrdinal = generationAttemptOrdinal;",
):
    require(QUANTIZED, needle, f"quantized accepted-attempt path changed: {needle}")
if "g_commitSerial" in MIGRATION or "g_commitSerial" in STATE:
    raise AssertionError("publication serial leaked into generation identity")

# UI migration is explicit: future Genre apply/G zeroes saved MORPH fields and
# exposes repeated G as reroll. Scene codec fields themselves are intentionally
# retained elsewhere for backwards decode.
for needle in (
    "settings.morphTarget = 0;",
    "settings.morphAmount = 0;",
    "requestedSettings.morphTarget = 0;",
    "requestedSettings.morphAmount = 0;",
    '"REROLL", "REPEAT G"',
):
    require(GENRE, needle, f"Genre MORPH migration changed: {needle}")

print("Combined F-02/F-07 seed surface source regressions: OK")
