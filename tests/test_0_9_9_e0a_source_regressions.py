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


generated = read("src/dsp/generated_phrase_song.h")
migration_h = read("src/generation/migration/strong_rhythm_migration.h")
migration_cpp = read("src/generation/migration/strong_rhythm_migration.cpp")
scenes = read("scenes.h")
phrase_types = read("src/phrase/phrase_types.h")
phrase_core = read("src/phrase/phrase_core.h")

# E0a coordinate contract: explicit physical Phrase ordinal, independent 4+4
# evolution segment ordinal, and the existing four-bar vocabulary-local ordinal.
require(
    migration_h,
    "constexpr uint8_t kGrooveVocabularyPhraseBars = 4;",
    "Groove Vocabulary identity bound must remain four bars",
)
require(
    migration_h,
    "constexpr uint8_t kUnspecifiedPhraseBarOrdinal = 0xFFu;",
    "non-Phrase compatibility sentinel must remain explicit",
)
require(
    migration_h,
    "uint8_t phraseBarOrdinal = kUnspecifiedPhraseBarOrdinal;",
    "Strong Rhythm migration context must carry Phrase-local bar identity",
)
require(
    migration_h,
    "uint8_t evolutionOrdinal = 0;",
    "Strong Rhythm migration context must carry an explicit evolution segment ordinal",
)
require(
    compact(migration_h),
    "static_cast<uint8_t>(phraseBarOrdinal/kGrooveVocabularyPhraseBars)",
    "evolutionOrdinal must be derived as four-bar segments",
)
require(
    compact(migration_h),
    "static_cast<uint8_t>(phraseBarOrdinal%kGrooveVocabularyPhraseBars)",
    "Groove Vocabulary local bar must wrap at four bars",
)

# generationAttemptOrdinal remains the retry/reroll coordinate; E0a must not
# overload it with Phrase temporal meaning.
require(
    migration_h,
    "uint32_t generationAttemptOrdinal = 0;",
    "generation attempt ordinal must remain a distinct context field",
)

# PREPARE owns coordinate construction and sends one explicit physical ordinal
# to both Atlas and procedural migration calls.
require(
    generated,
    "const uint8_t phraseBarOrdinal = static_cast<uint8_t>(barIndex);",
    "PREPARE must derive Phrase-local coordinate from its semantic bar loop",
)
require(
    compact(generated),
    "applyCurrentMigration(scene,genre,variation,phraseBarOrdinal,bar);",
    "Atlas PREPARE must pass the explicit Phrase-local ordinal",
)
require(
    compact(generated),
    "applyCurrentMigration(scene,genre,0,phraseBarOrdinal,migratedBase);",
    "procedural PREPARE must pass the explicit Phrase-local ordinal",
)
require(
    generated,
    "context.phraseBarOrdinal = coordinates.phraseBarOrdinal;",
    "PREPARE migration context must carry phraseBarOrdinal",
)
require(
    generated,
    "context.evolutionOrdinal = coordinates.evolutionOrdinal;",
    "PREPARE migration context must carry evolutionOrdinal",
)

prepare_body = between(generated, "inline bool prepare(", "template <typename Guard>")
raw_base_index = prepare_body.index("PhraseGenerator::PhraseBar proceduralBase{};")
bar_loop_index = prepare_body.index("for (int barIndex = 0; barIndex < bars; ++barIndex)")
copy_index = prepare_body.index("PhraseGenerator::PhraseBar migratedBase = proceduralBase;")
per_bar_migration_index = prepare_body.index("applyCurrentMigration(", copy_index)
derive_index = prepare_body.index("PhraseGenerator::deriveBar(", per_bar_migration_index)
if not (
    raw_base_index < bar_loop_index < copy_index < per_bar_migration_index < derive_index
):
    raise AssertionError(
        "procedural PREPARE must remain raw base once -> per-bar copy -> migration -> deriveBar"
    )

# The explicit evolution segment coordinate is context only in E0a. It must not
# be salted into migration RNG or otherwise manufacture output differences.
forbid(
    migration_cpp,
    "evolutionOrdinal",
    "E0a must not force fake variation from evolutionOrdinal",
)

# Non-Phrase callers keep the exact compatibility path: unspecified Phrase
# coordinate falls back to the pre-E0a patternAddress convention.
require(
    migration_cpp,
    "if (context.phraseBarOrdinal != kUnspecifiedPhraseBarOrdinal)",
    "explicit Phrase coordinate must be opt-in",
)
require(
    migration_cpp,
    "settings.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi)",
    "legacy LoFi address-sensitive compatibility path must remain",
)
require(
    migration_cpp,
    "settings.generativeMode == static_cast<uint8_t>(GenerativeMode::HipHop)",
    "legacy HipHop address-sensitive compatibility path must remain",
)
require(
    migration_cpp,
    "settings.generativeMode == static_cast<uint8_t>(GenerativeMode::FunkSoul)",
    "legacy FunkSoul address-sensitive compatibility path must remain",
)
require(
    migration_cpp,
    "static_cast<uint8_t>(context.patternAddress & 0xFF)",
    "legacy patternAddress fallback must remain",
)

# Existing bar-sensitive vocabulary is the consumer. E0a introduces no new
# trajectory vocabulary.
for request in (
    "bassRequest.barOrdinal = barOrdinal;",
    "pitchRequest.barOrdinal = barOrdinal;",
    "chordRequest.barOrdinal = barOrdinal;",
    "melodicRequest.barOrdinal = barOrdinal;",
):
    require(migration_cpp, request, "existing semantic role must consume local bar ordinal")

# Physical Pattern remains exactly one 16-step bar.
require(scenes, "struct DrumPattern {", "Drum Pattern type must remain present")
require(scenes, "struct SynthPattern {", "Synth Pattern type must remain present")
if scenes.count("static constexpr int kSteps = 16;") < 2:
    raise AssertionError("SynthPattern and DrumPattern must both remain 16-step one-bar values")

# Phrase remains the bounded 1/2/4/8 multi-bar reference carrier.
require(phrase_types, "constexpr uint8_t kMaxBars = 8;", "Phrase physical carrier bound")
require(
    phrase_types,
    "int16_t patternRefs[kMaxBars][kTrackCount]{};",
    "Phrase must carry ordered Pattern references",
)
for length in ("bars == 1", "bars == 2", "bars == 4", "bars == 8"):
    require(phrase_types, length, "Phrase must retain 1/2/4/8 length contract")
require(
    phrase_core,
    "candidate.patternRefs[bar][track] = reference;",
    "Phrase capture must materialize Song references into Phrase refs",
)
require(
    phrase_core,
    "destination.positions[startRow + bar].patterns[track] =",
    "Phrase write must project references back into Song arrangement rows",
)

# Song remains arrangement/reference owner; PREPARE staging does not acquire a
# persistent temporal cursor or a second lifecycle state machine.
require(
    generated,
    "Song& song = scene.songs[prepared.songSlot];",
    "generated Phrase commit must continue to target Song arrangement state",
)
require(
    generated,
    "SongPosition& position =",
    "generated Phrase commit must continue to publish Song row references",
)
prepared_block = between(
    generated,
    "struct PreparedPhraseArrangement {",
    "struct GeneratedPhraseUndoPayload {",
)
forbid(
    prepared_block,
    "phraseBarOrdinal",
    "Phrase temporal coordinates are PREPARE inputs, not staged lifecycle state",
)
forbid(
    prepared_block,
    "evolutionOrdinal",
    "evolution coordinate must not become staged lifecycle state",
)

# Existing C/D2/D3 lifecycle remains authoritative.
for lifecycle_call in (
    "PhraseLiveArrangementDetail::armPhraseActivation(",
    "undoOwner().commitPrepared(",
    "PhraseLiveArrangementDetail::completePhraseActivation(",
):
    require(generated, lifecycle_call, "existing PREPARE/COMMIT/ACTIVATE lifecycle must remain")

combined_production = generated + migration_h + migration_cpp
forbidden_architecture = (
    "E0aScheduler",
    "PhraseScheduler",
    "TemporalScheduler",
    "EvolutionScheduler",
    "PatternLease",
    "EVOLVE NEXT",
)
for symbol in forbidden_architecture:
    forbid(combined_production, symbol, "E0a production scope must remain frozen")

print("0.9.9-E0a source regressions: PASS")
