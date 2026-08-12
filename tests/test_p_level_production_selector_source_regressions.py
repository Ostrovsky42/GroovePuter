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
# must not stop/restart transport to hide an in-place mutation: current material
# remains active and the prepared A+B+Drums transaction publishes at BAR_START.
require(
    GENRE,
    "regenerateWithQuantizedCommit(",
    "GENRE no longer routes full generation through quantized commit",
)
require(GENRE, 'resultLabel = "GEN -> NEXT BAR";', "pending GEN feedback disappeared")
if "mini_acid_.stop();" in GENRE or "mini_acid_.start();" in GENRE:
    raise AssertionError("GENRE generation reintroduced transport stop/restart")

for needle in (
    "if (!engine.isPlaying())",
    "QuantizedGenerationResult::CommittedNow",
    "QuantizedGenerationResult::PendingNextBar",
    "const SynthPattern activeSynthA",
    "const SynthPattern activeSynthB",
    "const DrumPatternSet activeDrums",
    "scenes.editCurrentSynthPattern(0) = activeSynthA;",
    "scenes.editCurrentSynthPattern(1) = activeSynthB;",
    "scenes.editCurrentDrumPattern() = activeDrums;",
    "g_pendingValid.store(true, std::memory_order_release);",
    "setPendingCommitHook(&commitQuantizedGenerationAtBarStart)",
    "targetStillActive(scenes, g_pending.target)",
    "GroovePuterState::markSceneMutated();",
):
    require(QUANTIZED, needle, f"quantized generation contract changed: {needle}")

# Heavy Stage 15 generation must happen only on the control-side preparation
# path. The existing audio BAR_START call remains a bounded callback bridge; its
# boolean return stays false so MiniAcid never falls through to the historical
# audio-thread regeneratePatternsWithGenre() branch.
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

for forbidden in ("std::vector", "std::string", "new ", "malloc(", "calloc("):
    if forbidden in QUANTIZED:
        raise AssertionError(
            f"quantized generation pending state is no longer allocation-free: {forbidden}"
        )

print("P-level production selector + quantized generation source regressions: OK")
