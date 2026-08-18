from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")
BRIDGE = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
QUANTIZED = (
    ROOT / "src/generation/migration/quantized_generation_commit_impl.h"
).read_text(encoding="utf-8")
GENRE_MANAGER = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (
    ROOT / "src/ui/pages/drum_sequencer_page_legacy.h"
).read_text(encoding="utf-8")
PATTERN = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def function_body(text: str, signature: str, next_signature: str) -> str:
    require(text, signature, f"missing function boundary: {signature}")
    body = text.split(signature, 1)[1]
    if next_signature in body:
        body = body.split(next_signature, 1)[0]
    return body


def without_line_comments(text: str) -> str:
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


# P-level has one runtime/session owner. P2 is the compatibility default so
# upgrading the firmware cannot silently alter existing generation behavior.
for needle in (
    "RealizationLevel::P2Variation",
    "currentGenerationLevel()",
    "cycleGenerationLevel(int direction = 1)",
    "generationLevelCode(",
    'return "P1";',
    'return "P2";',
    'return "P3";',
    'return "P1 CANON";',
    'return "P2 VAR";',
    'return "P3 TRANS";',
    "Persistence is deliberately deferred",
):
    require(STATE, needle, f"P-level request owner changed: {needle}")

# F-07 shares the same runtime/session owner but not the same axis. The table is
# a bounded, allocation-free history cache: capacity may forget an old tuple but
# must never reject a newly accepted generation request.
for needle in (
    "GenerationAttemptStatus",
    "GenerationAttemptAllocation",
    "GenerationAttemptStatus::InvalidTuple",
    "kGenerationAttemptCapacity = 64",
    "generation attempt table memory contract",
    "allocateGenerationAttempt(",
    "resetGenerationAttemptState()",
    "attemptVictimStorage()",
    "GenerationAttemptEntry entries[kGenerationAttemptCapacity]{}",
    "return {GenerationAttemptStatus::Ok, 0};",
):
    require(STATE, needle, f"reroll request owner changed: {needle}")
for forbidden in (
    "GenerationAttemptStatus::TableFull",
    "GenerationAttemptStatus::OrdinalExhausted",
):
    if forbidden in STATE:
        raise AssertionError(f"reroll history capacity became a generation failure again: {forbidden}")
state_code = without_line_comments(STATE)
for forbidden in (
    "Preferences",
    "putUChar",
    "putBytes",
    "gp-generation",
    "std::vector",
    "std::map",
    "unordered_map",
    "new ",
    "malloc(",
):
    if forbidden in state_code:
        raise AssertionError(f"generation request state gained persistence/heap: {forbidden}")

# The generic live bridge consumes the one shared P-level and allocates an
# attempt only for callers that did not already accept it in the quantized owner.
for needle in (
    "context.level = GroovePuterState::currentGenerationLevel();",
    "assignGenerationAttempt(scene.genre, context",
    "GroovePuterState::allocateGenerationAttempt(",
    "context.generationAttemptOrdinal = allocation.ordinal;",
    "result.level = baseContext.level;",
    "request.level = baseContext.level;",
    "context.level,",
    '"[PHRASE-PROBE] status=%s level=%s',
    "GroovePuterState::generationLevelCode(result.level)",
):
    require(BRIDGE, needle, f"P-level/reroll live bridge contract changed: {needle}")

# The fixed P2/P3 values in runSubtractiveRuntimeProbe are deliberate capability
# benchmarks. They are not the current production request level and must not be
# generalized into a second selector.
require(
    BRIDGE,
    "reduction.level = RealizationLevel::P2Variation;",
    "P2 reduction benchmark disappeared",
)
require(
    BRIDGE,
    "broken.level = RealizationLevel::P3Transformation;",
    "P3 break benchmark disappeared",
)

# P is reachable from both GENERATE pages and the main DRUMS grid, always through
# the same session owner. Cardputer may deliver a printable key or only scancode.
for name, source in (("GENRE", GENRE), ("FEEL", FEEL), ("DRUMS", DRUM)):
    require(
        source,
        "GroovePuterState::cycleGenerationLevel()",
        f"{name} no longer cycles the shared P-level",
    )
    require(
        source,
        "GroovePuterState::generationLevelShortName(level)",
        f"{name} no longer reports the selected P-level",
    )
    require(
        source,
        "GROOVEPUTER_P",
        f"{name} no longer recognizes scancode-only P",
    )
    if "CONTINUE: Ctrl+Alt+G" in source:
        raise AssertionError(f"{name} still treats plain P as continuation")

# Existing generation ownership remains separate: normal generation is G,
# phrase audition is Ctrl+Alt+G, and CHAOS stays outside P1/P2/P3/reroll.
# R9 keeps DRUMS G musically drums-only Strong Rhythm, but its persistent
# publication now goes through the canonical bounded generation COMMIT so the
# one retained Ctrl+Z slot can exchange OLD <-> GENERATED.
require(GENRE, "applyCurrent(true);", "GENRE G production route disappeared")
require(GENRE, '"REROLL", "REPEAT G"', "GENRE reroll affordance disappeared")
require(
    DRUM,
    "regenerateDrumsWithQuantizedCommit",
    "DRUMS G production route disappeared",
)
require(
    DRUM,
    "regeneratePhraseAuditionWithProbe",
    "Ctrl+Alt+G phrase audition route disappeared",
)
require(
    DRUM_LEGACY,
    "mini_acid_.randomizeDrumPatternChaos();",
    "Alt+G CHAOS route disappeared",
)
if "cycleGenerationLevel" in DRUM_LEGACY:
    raise AssertionError("legacy Ctrl/Alt+G handler became a second P-level owner")

# Full GENRE materialization is quantized while transport is running. The page
# must neither stop/restart transport nor hold AudioMutationGate while the heavy
# candidate is prepared. AudioTask continues rendering the current bar.
for needle in (
    "regenerateWithQuantizedCommit(",
    "if (doRegenerate && mini_acid_.isPlaying())",
    "AudioTask keeps rendering the current bar",
    'resultLabel = "GEN -> NEXT BAR";',
):
    require(GENRE, needle, f"GENRE quantized route changed: {needle}")
if "mini_acid_.stop();" in GENRE or "mini_acid_.start();" in GENRE:
    raise AssertionError("GENRE generation reintroduced transport stop/restart")

playing_branch = GENRE.split(
    "if (doRegenerate && mini_acid_.isPlaying())", 1
)[1].split("} else {", 1)[0]
if "withAudioGuard" in playing_branch:
    raise AssertionError("PLAY generation still pauses AudioTask through AudioGuard")

require(PATTERN, "regenerateSynthWithQuantizedCommit(",
        "SYNTH G lost the Genre-aware synth-only owner")

# Pending publication remains a fixed-size double buffer. Repeated G may reclaim
# Ready (newest intent); attempt identity is assigned before preparation and is
# carried by the request even if that publication is later cancelled/superseded.
for needle in (
    "inline PendingGeneration g_slots[2]{};",
    "SlotState::Writing",
    "SlotState::Ready",
    "SlotState::Reading",
    "g_publishedSlot.exchange(-1, std::memory_order_acq_rel)",
    "WriteLease acquireWriteLease()",
    "lease.hasPreviousPending",
    "publishWriteSlot(lease.slot);",
    "targetStillActive(scenes, target)",
    "targetStillActive(scenes, pending.target)",
    "setPendingCommitHook(&commitQuantizedGenerationAtBarStart)",
    "QuantizedGenerationResult::PendingNextBar",
    "QuantizedGenerationResult::CommittedNow",
    "QuantizedGenerationResult::AttemptUnavailable",
    "allocateAttemptFor(requestedGenre, requestLevel, target, attemptOrdinal)",
):
    require(QUANTIZED, needle, f"lock-free quantized/reroll contract changed: {needle}")

# PLAY preparation must be scratch-only and receive the already accepted
# P-level/attempt. It must not mutate live Scene/DSP before publication.
prepare = function_body(
    QUANTIZED,
    "inline bool preparePlayingCandidate(",
    "}  // namespace QuantizedGenerationDetail",
)
for needle in (
    "RealizationLevel requestLevel",
    "uint32_t generationAttemptOrdinal",
    "AtlasRuntime::applyRecipe(",
    "GrooveboxModeManager scratchMode(engine);",
    "scratchMode.setModeLocal(requestedMode);",
    "scratchMode.setGenerationSeed(engine.modeManager().generationSeed());",
    "scratchMode.generatePattern(",
    "scratchMode.generateDrumPattern(",
    "context.level = requestLevel;",
    "context.generationAttemptOrdinal = generationAttemptOrdinal;",
    "migrateStrongRhythmMaterial(",
):
    require(prepare, needle, f"PLAY scratch request lost: {needle}")
for forbidden in (
    "scene.genre =",
    "engine.setGrooveboxMode(",
    "engine.setBpm(",
    "editCurrentSynthPattern(",
    "editCurrentDrumPattern(",
    "allocateGenerationAttempt(",
    "regenerateWithStrongRhythmMigration(",
):
    if forbidden in prepare:
        raise AssertionError(f"PLAY preparation mutates/reallocates request: {forbidden}")

# STOP remains immediate but now allocates before the first live mutation and
# performs migration directly with that exact ordinal to avoid double allocation.
stop = QUANTIZED.split("if (!engine.isPlaying())", 1)[1].split(
    "// PLAY preparation", 1
)[0]
for needle in (
    "allocateAttemptFor(requestedGenre, requestLevel, target, attemptOrdinal)",
    "scene.genre = requestedGenre;",
    "engine.regeneratePatternsWithGenre();",
    "context.level = requestLevel;",
    "context.generationAttemptOrdinal = attemptOrdinal;",
    "migrateStrongRhythmMaterial(",
):
    require(stop, needle, f"STOP immediate/reroll contract changed: {needle}")
if "regenerateWithStrongRhythmMigration(engine);" in stop:
    raise AssertionError("STOP double-allocates accepted reroll through live bridge")

# Heavy generation must never run from audio BAR_START. Commit serial is allowed
# only as publication telemetry; it must never be consumed by generation RNG.
require(
    ENGINE,
    "if (genreManager_.commitPendingRecipe()) {",
    "real BAR_START commit hook disappeared",
)
require(
    GENRE_MANAGER,
    "if (pendingCommitHook_ != nullptr) pendingCommitHook_(scenes_);",
    "Genre BAR_START compatibility hook no longer invokes pending commit",
)
require(
    GENRE_MANAGER,
    "return false;",
    "BAR_START hook may fall through into heavy audio-thread generation",
)

commit = function_body(
    QUANTIZED,
    "inline bool commitQuantizedGenerationAtBarStart(",
    "inline QuantizedGenerationResult regenerateWithQuantizedCommit(",
)
for forbidden in (
    "AtlasRuntime::applyRecipe(",
    "generatePattern(",
    "generateDrumPattern(",
    "migrateStrongRhythmMaterial(",
    "allocateGenerationAttempt(",
    "regenerateWithStrongRhythmMigration(",
):
    if forbidden in commit:
        raise AssertionError(f"BAR_START performs heavy generation/request allocation: {forbidden}")

for forbidden in ("std::vector", "std::string", "new ", "malloc(", "calloc("):
    if forbidden in QUANTIZED:
        raise AssertionError(
            f"quantized generation pending state is no longer allocation-free: {forbidden}"
        )

require(
    QUANTIZED,
    "GroovePuterState::markSceneMutated();",
    "successful BAR_START commit no longer publishes Scene revision",
)

if "g_commitSerial" in STATE:
    raise AssertionError("generation request state must not depend on commit serial")
if "morph_amount_" in GENRE:
    raise AssertionError("retired MORPH UI must not remain in GENRE body")
if "FocusRow::Morph" in GENRE:
    raise AssertionError("retired MORPH focus row must not remain in GENRE body")

print("P-level + bounded reroll + lock-free quantized generation regressions: OK")
