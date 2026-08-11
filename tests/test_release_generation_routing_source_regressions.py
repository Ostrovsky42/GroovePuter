from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
GENRE_H = (ROOT / "src/ui/pages/genre_page.h").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (
    ROOT / "src/ui/pages/drum_sequencer_page_legacy.h"
).read_text(encoding="utf-8")
BRIDGE = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
SKETCH = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# GENRE plain G is explicit full materialization, independent of ENTER's APPLY
# selector. It commits the pending GENRE / VARIANT / RHYTHM settings first and
# uses the same Stage 15 bridge as normal full regeneration.
for needle in (
    "void GenrePage::applyCurrent(bool forceRegenerate)",
    "forceRegenerate || applyMode != ApplyMode::ProfileOnly",
    "regenerateWithStrongRhythmMigration(mini_acid_)",
    "const bool keyG = key == 'g' || event.scancode == GROOVEPUTER_G;",
    "if (keyG && !event.ctrl && !event.alt && !event.meta)",
    "applyCurrent(true);",
    '"G:GEN P:LEVEL M:MODE"',
):
    require(GENRE, needle, f"GENRE G release route changed: {needle}")

require(
    GENRE_H,
    "void applyCurrent(bool forceRegenerate = false);",
    "GenrePage lost explicit forced-regeneration API",
)
for forbidden in ("randomize303Pattern", "randomizeDrumPattern", "modeManager_"):
    if forbidden in GENRE:
        raise AssertionError(f"GENRE page gained legacy generation owner: {forbidden}")

# The two GENERATE pages still consume stale I/O before Cardputer's retained
# sketch-level compatibility fallback can reach GrooveboxModeManager. P is now
# separately owned by the P1/P2/P3 request selector rather than continuation.
for source, name in ((GENRE, "GENRE"), (FEEL, "FEEL")):
    require(source, '"LEGACY SYNTH GEN OFF"',
            f"{name} legacy synth-generation guard disappeared")
    require(source, "GroovePuterState::cycleGenerationLevel()",
            f"{name} lost page-first P-level ownership")

# DRUMS plain G remains drums-only strong generation. Ctrl+Alt+G is the
# multi-bar audition. Ctrl+G and Alt+G stay distinct edit/chaos commands.
for needle in (
    "if (keyG && ui_event.ctrl && ui_event.alt && !ui_event.meta)",
    "regeneratePhraseAuditionWithProbe",
    "if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)",
    "regenerateDrumsWithStrongRhythmMigration",
    "GroovePuterState::cycleGenerationLevel()",
    '"LEGACY O GEN OFF"',
):
    require(DRUM, needle, f"DRUMS release route changed: {needle}")

require(
    DRUM_LEGACY,
    "mini_acid_.randomizeDrumVoice(voice);",
    "Ctrl+G selected-voice randomize disappeared",
)
require(
    DRUM_LEGACY,
    "mini_acid_.randomizeDrumPatternChaos();",
    "Alt+G chaos route disappeared",
)

# The sketch compatibility fallback still exists for non-release surfaces. Pin
# it explicitly so nobody mistakes the stacked selector PR for full
# legacy-generator deletion. Release safety comes from page-first ownership.
for needle in (
    "g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false",
    "if (handled)",
    "g_miniAcid->randomize303Pattern(0);",
    "g_miniAcid->randomize303Pattern(1);",
    "g_miniAcid->randomizeDrumPattern();",
):
    require(SKETCH, needle, f"sketch fallback boundary changed: {needle}")

# Stage 15 tonal context is shared by full G and restored multi-bar audition.
for needle in (
    "context.tonalMaterializationEnabled = true;",
    "context.rootPitchClass",
    "context.scaleTypeValue",
    "migrateStrongRhythmMaterial(",
):
    require(BRIDGE, needle, f"shared Stage 15 tonal route changed: {needle}")

print("Release generation routing source regressions: OK")
