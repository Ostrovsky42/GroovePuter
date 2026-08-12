from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")
BRIDGE = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
QUANTIZED = (
    ROOT / "src/generation/migration/quantized_generation_commit.h"
).read_text(encoding="utf-8")
GENRE_MANAGER = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (
    ROOT / "src/ui/pages/drum_sequencer_page_legacy.h"
).read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def function_body(text: str, signature: str, next_signature: str) -> str:
    require(text, signature, f"missing function boundary: {signature}")
    body = text.split(signature, 1)[1]
    if next_signature in body:
        body = body.split(next_signature, 1)[0]
    return body


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

# P is a realtime input action. Synchronous flash/NVS writes must not enter this
# path; any future persistence belongs in a deferred session service.
for forbidden in ("Preferences", "putUChar", "putBytes", "gp-generation"):
    if forbidden in STATE:
        raise AssertionError(f"P-level selector performs/depend on NVS I/O: {forbidden}")

# The production bridge must consume the one shared request level. There must be
# no separate live-G or audition P-level owner.
for needle in (
    "context.level = GroovePuterState::currentGenerationLevel();",
    "result.level = baseContext.level;",
    "request.level = baseContext.level;",
    "context.level,",
    '"[PHRASE-PROBE] status=%s level=%s',
    "GroovePuterState::generationLevelCode(result.level)",
):
    require(BRIDGE, needle, f"P-level live bridge contract changed: {needle}")

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
# the same session owner. Cardputer may deliver a printable key or only scancode,
# so every public P-level surface must accept GROOVEPUTER_P as well.
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

# Existing generation ownership remains separate: normal generation is still G,
# phrase continuation/audition is Ctrl+Alt+G, and CHAOS stays outside P1/P2/P3.
require(GENRE, "applyCurrent(true);", "GENRE G production route disappeared")
require(
    DRUM,
    "regenerateDrumsWithStrongRhythmMigration",
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
# candidate is prepared. AudioTask must continue rendering the current bar.
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

# Pending publication is a fixed-size double buffer. The control side owns only
# Writing slots; BAR_START claims an immutable Ready slot as Reading. Repeated G
# reclaims the currently Ready slot when possible so newest intent wins without
# racing an already-running BAR_START commit.
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
):
    require(QUANTIZED, needle, f"lock-free quantized contract changed: {needle}")

# PLAY preparation must be scratch-only: Atlas or a private ModeManager creates
# candidate A/B/Drums, then the pure Stage 15 materializer transforms those local
# values. It must not write live Scene patterns, mode, BPM or call the active
# legacy bridge before publication.
prepare = function_body(
    QUANTIZED,
    "inline bool preparePlayingCandidate(",
    "}  // namespace QuantizedGenerationDetail",
)
for needle in (
    "AtlasRuntime::applyRecipe(",
    "GrooveboxModeManager scratchMode(engine);",
    "scratchMode.setModeLocal(requestedMode);",
    "scratchMode.setGenerationSeed(engine.modeManager().generationSeed());",
    "scratchMode.generatePattern(",
    "scratchMode.generateDrumPattern(",
    "migrateStrongRhythmMaterial(",
):
    require(prepare, needle, f"PLAY scratch generation lost: {needle}")
for forbidden in (
    "scene.genre =",
    "engine.setGrooveboxMode(",
    "engine.setBpm(",
    "editCurrentSynthPattern(",
    "editCurrentDrumPattern(",
    "regenerateWithStrongRhythmMigration(",
):
    if forbidden in prepare:
        raise AssertionError(f"PLAY preparation mutates live runtime: {forbidden}")

# STOP remains immediate and may use the active bridge under the caller's guard.
for needle in (
    "if (!engine.isPlaying())",
    "scene.genre = requestedGenre;",
    "regenerateWithStrongRhythmMigration(engine);",
):
    require(QUANTIZED, needle, f"STOP immediate contract changed: {needle}")

# Heavy generation must never run from the audio BAR_START path. The existing
# transport callback invokes only the bounded commit hook; its boolean remains
# false so MiniAcid cannot fall through to historical audio-thread regeneration.
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
    "regenerateWithStrongRhythmMigration(",
):
    if forbidden in commit:
        raise AssertionError(f"BAR_START performs heavy generation: {forbidden}")

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

print("P-level production selector + lock-free quantized generation regressions: OK")
