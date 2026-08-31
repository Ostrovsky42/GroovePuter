#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(f"{message}: missing {needle!r}")


def forbid(text: str, needle: str, message: str) -> None:
    if needle in text:
        raise AssertionError(f"{message}: unexpected {needle!r}")


def between(text: str, start: str, end: str) -> str:
    start_index = text.index(start)
    end_index = text.index(end, start_index)
    return text[start_index:end_index]


def compact(text: str) -> str:
    return "".join(text.split())


activity = read("src/generation/rhythm/evolution_activity.h")
migration = read("src/generation/migration/strong_rhythm_migration.h")
scenes = read("scenes.h")
generated = read("src/dsp/generated_phrase_song.h")
phrase_evolution = read("src/generation/phrase/phrase_evolution.cpp")

# E2t consumes the explicit E0a temporal coordinate meaning. The policy keeps
# the exact four-bar evolution-segment interpretation instead of deriving a
# cadence coordinate from Pattern/Song transport identity.
require(
    migration,
    "constexpr uint8_t kGrooveVocabularyPhraseBars = 4;",
    "E0a four-bar temporal bound must remain authoritative",
)
require(
    compact(migration),
    "static_cast<uint8_t>(phraseBarOrdinal/kGrooveVocabularyPhraseBars)",
    "E0a evolutionOrdinal must remain phraseBarOrdinal / 4",
)
require(
    activity,
    "constexpr uint8_t kEvolutionCadenceSegmentBars = 4;",
    "E2t cadence must preserve the E0a four-bar segment bound",
)
require(
    activity,
    "uint8_t phraseBarOrdinal,",
    "cadence policy must consume explicit Phrase-local bar coordinate",
)
require(
    activity,
    "uint8_t evolutionOrdinal,",
    "cadence policy must consume explicit evolution segment coordinate",
)
require(
    compact(activity),
    "phraseBarOrdinal%kEvolutionCadenceSegmentBars!=0u",
    "cadence decisions must occur only at four-bar segment boundaries",
)
require(
    compact(activity),
    "phraseBarOrdinal/kEvolutionCadenceSegmentBars!=evolutionOrdinal",
    "inconsistent E0a temporal coordinates must fail closed",
)

# Activity is a bounded frequency policy. P1/P2/P3 remain the independent
# realization-depth axis and are intentionally absent from this API.
for level in ("Off", "Low", "Medium", "High", "Count"):
    require(activity, level, "bounded EvolutionActivity vocabulary")
for result in ("Hold", "Attempt"):
    require(activity, result, "minimal cadence result vocabulary")
for unrelated in (
    "RealizationLevel",
    "P1Canonical",
    "P2Variation",
    "P3Transformation",
    "PhraseEvolutionLawId",
    "BarFunction",
):
    forbid(activity, unrelated, "cadence must not answer HOW FAR or trajectory shape")

decision_block = between(
    activity,
    "enum class EvolutionCadenceDecision",
    "constexpr bool isValidEvolutionActivity",
)
for premature_lifecycle_term in ("Keep", "Revert", "Evolve"):
    forbid(
        decision_block,
        premature_lifecycle_term,
        "E2t result must remain HOLD/ATTEMPT only",
    )

# Determinism is driven only by explicit Activity, E0a coordinates, and stable
# generation seed/context. Retry identity and physical destination identity are
# not available to the pure policy.
require(
    activity,
    "const GenerationContext& generation",
    "cadence policy must take stable generation context explicitly",
)
require(
    activity,
    "generation.projectSeed",
    "project seed must participate in deterministic cadence",
)
require(
    activity,
    "generation.phraseOrdinal",
    "phrase identity context must participate in deterministic cadence",
)
require(
    activity,
    "deterministicValue(",
    "cadence must use the existing deterministic value primitive",
)
for forbidden_coordinate in (
    "patternAddress",
    "songBarIndex",
    "songPosition",
    "generationAttemptOrdinal",
):
    forbid(
        activity,
        forbidden_coordinate,
        "cadence must not derive from destination/transport/retry identity",
    )

# No persisted/UI owner is introduced. GenreSettings remains unchanged and the
# new Activity type is transient policy input only.
genre_settings = between(scenes, "struct GenreSettings {", "struct DrumFX {")
for persisted_symbol in ("EvolutionActivity", "evolutionActivity", "activity"):
    forbid(
        genre_settings,
        persisted_symbol,
        "E2t Activity must not become Scene persistence/UI state",
    )

# This checkpoint deliberately does not wire a scheduler, lifecycle decision,
# mutation executor, PREPARE owner, or candidate chooser.
forbidden_architecture = (
    "Scheduler",
    "schedule",
    "Mutation",
    "mutation",
    "candidate",
    "Candidate",
    "commit",
    "Commit",
    "activate",
    "Activate",
    "transport",
    "Transport",
)
for symbol in forbidden_architecture:
    forbid(activity, symbol, "E2t policy scope must remain pure and bounded")

for production_owner in (generated, phrase_evolution):
    forbid(
        production_owner,
        "evolution_activity.h",
        "E2t must not wire cadence into PREPARE or evolution execution yet",
    )
    forbid(
        production_owner,
        "evolutionCadenceDecision(",
        "E2t must not add execution/lifecycle wiring",
    )

print("0.9.9-E2t source regressions: PASS")
