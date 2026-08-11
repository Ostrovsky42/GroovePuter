from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")
BRIDGE = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (
    ROOT / "src/ui/pages/drum_sequencer_page_legacy.h"
).read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# P-level has one request/session owner. P2 is the compatibility default so
# upgrading the firmware cannot silently alter existing generation behavior.
for needle in (
    "RealizationLevel::P2Variation",
    'preferences.begin("gp-generation", true)',
    '"p-level"',
    "currentGenerationLevel()",
    "cycleGenerationLevel(int direction = 1)",
    'return "P1 CANON";',
    'return "P2 VAR";',
    'return "P3 TRANS";',
):
    require(STATE, needle, f"P-level request owner changed: {needle}")

# The production bridge must consume the one shared request level. There must be
# no separate live-G or audition P-level owner.
for needle in (
    "context.level = GroovePuterState::currentGenerationLevel();",
    "result.level = baseContext.level;",
    "request.level = baseContext.level;",
    "context.level,",
    '"[PHRASE-PROBE] status=%s level=%s',
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
# the same session owner. The temporary 'P means continuation' guard is gone.
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

print("P-level production selector source regressions: OK")
