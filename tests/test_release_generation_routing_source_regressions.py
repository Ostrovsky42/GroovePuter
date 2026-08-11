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
    '"ENTER:Apply G:Gen M:Mode"',
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

# The two GENERATE pages consume stale I/O/P before Cardputer's retained
# sketch-level compatibility fallback can reach GrooveboxModeManager. P points
# to the explicit audition command instead of silently replacing the drums.
for needle in (
    "(key == 'i' || key == 'o' || key == 'p')",
    '"CONTINUE: Ctrl+Alt+G"',
    '"LEGACY SYNTH GEN OFF"',
):
    require(GENRE, needle, f"GENRE legacy-generation guard changed: {needle}")

for needle in (
    "event.key == 'i' || event.key == 'I'",
    "event.key == 'o' || event.key == 'O'",
    "event.key == 'p' || event.key == 'P'",
    '"CONTINUE: Ctrl+Alt+G"',
    '"LEGACY SYNTH GEN OFF"',
):
    require(FEEL, needle, f"FEEL legacy-generation guard changed: {needle}")

# DRUMS plain G remains drums-only strong generation. Ctrl+Alt+G is the
# multi-bar audition. Ctrl+G and Alt+G stay distinct edit/chaos commands.
for needle in (
    "if (keyG && ui_event.ctrl && ui_event.alt && !ui_event.meta)",
    "regeneratePhraseAuditionWithProbe",
    "if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)",
    "regenerateDrumsWithStrongRhythmMigration",
    "(lowerKey == 'o' || lowerKey == 'p')",
    '"CONTINUE: Ctrl+Alt+G"',
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
# it explicitly so nobody mistakes this PR for full legacy-generator deletion.
# Release safety comes from the page-first guards above.
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
