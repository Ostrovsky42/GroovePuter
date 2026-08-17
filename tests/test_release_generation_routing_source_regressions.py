from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
GENRE_H = (ROOT / "src/ui/pages/genre_page.h").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (
    ROOT / "src/ui/pages/drum_sequencer_page_legacy.h"
).read_text(encoding="utf-8")
PATTERN = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
BRIDGE = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
QUANTIZED = (
    ROOT / "src/generation/migration/quantized_generation_commit_impl.h"
).read_text(encoding="utf-8")
SKETCH = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# GENRE plain G is explicit full materialization, independent of ENTER's APPLY
# selector. It prepares the pending GENRE / VARIANT / RHYTHM state and routes
# through the quantized owner.
for needle in (
    "void GenrePage::applyCurrent(bool forceRegenerate)",
    "forceRegenerate || applyMode != ApplyMode::ProfileOnly",
    "regenerateWithQuantizedCommit(",
    "if (doRegenerate && mini_acid_.isPlaying())",
    "const bool keyG = key == 'g' || event.scancode == GROOVEPUTER_G;",
    "if (keyG && !event.ctrl && !event.alt && !event.meta)",
    "applyCurrent(true);",
    '"G:GEN P:LEVEL M:MODE"',
    '"REROLL"',
    '"REPEAT G"',
):
    require(GENRE, needle, f"GENRE G release route changed: {needle}")

# F-02/F-07: STOP and PLAY both allocate the request identity before live
# publication/mutation. STOP then runs legacy rollback + Stage15 migration
# directly with that assigned ordinal; calling the generic live bridge here
# would allocate the same accepted request twice.
for needle in (
    "allocateAttemptFor(",
    "engine.regeneratePatternsWithGenre();",
    "context.generationAttemptOrdinal = attemptOrdinal;",
    "migrateStrongRhythmMaterial(",
    "GrooveboxModeManager scratchMode(engine);",
    "QuantizedGenerationResult::PendingNextBar",
    "QuantizedGenerationResult::AttemptUnavailable",
):
    require(QUANTIZED, needle, f"quantized Stage15/reroll route changed: {needle}")

stop_block = QUANTIZED.split("if (!engine.isPlaying())", 1)[1].split(
    "// PLAY preparation", 1
)[0]
for needle in (
    "allocateAttemptFor(requestedGenre, requestLevel, target, attemptOrdinal)",
    "scene.genre = requestedGenre;",
    "engine.regeneratePatternsWithGenre();",
    "context.generationAttemptOrdinal = attemptOrdinal;",
    "migrateStrongRhythmMaterial(",
):
    require(stop_block, needle, f"STOP accepted-attempt contract changed: {needle}")
if "regenerateWithStrongRhythmMigration(engine);" in stop_block:
    raise AssertionError("quantized STOP path double-allocates through live bridge")

require(
    GENRE_H,
    "void applyCurrent(bool forceRegenerate = false);",
    "GenrePage lost explicit forced-regeneration API",
)
for forbidden in (
    "randomize303Pattern",
    "randomizeDrumPattern",
    "modeManager_",
    "adjustMorph",
    "morphAccelerator",
    "morph_amount_",
    "FocusRow::Morph",
):
    if forbidden in GENRE + GENRE_H:
        raise AssertionError(f"GENRE page gained retired/legacy generation owner: {forbidden}")
for needle in (
    "settings.morphTarget = 0;",
    "settings.morphAmount = 0;",
    "requestedSettings.morphTarget = 0;",
    "requestedSettings.morphAmount = 0;",
):
    require(GENRE, needle, f"persisted MORPH migration changed: {needle}")

# The two GENERATE pages still consume stale I/O before Cardputer's retained
# sketch-level compatibility fallback can reach GrooveboxModeManager. P is now
# separately owned by the P1/P2/P3 request selector rather than continuation.
for source, name in ((GENRE, "GENRE"), (FEEL, "FEEL")):
    require(source, '"LEGACY SYNTH GEN OFF"',
            f"{name} legacy synth-generation guard disappeared")
    require(source, "GroovePuterState::cycleGenerationLevel()",
            f"{name} lost page-first P-level ownership")

# DRUMS plain G remains drums-only Strong Rhythm generation. R9 adds the
# canonical bounded generation COMMIT around that same musical materialization
# so Ctrl+Z can exchange OLD <-> GENERATED. Ctrl+Alt+G is the multi-bar audition;
# Ctrl+G and Alt+G stay distinct edit/chaos commands.
for needle in (
    "if (keyG && ui_event.ctrl && ui_event.alt && !ui_event.meta)",
    "regeneratePhraseAuditionWithProbe",
    "if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)",
    "regenerateDrumsWithQuantizedCommit",
    "GroovePuterState::cycleGenerationLevel()",
    '"LEGACY O GEN OFF"',
):
    require(DRUM, needle, f"DRUMS release route changed: {needle}")

require(PATTERN, "regenerateSynthWithQuantizedCommit(",
        "SYNTH plain G bypasses Genre-aware synth generation")
require(PATTERN, "if (!note_entry_mode_ && keyG &&",
        "SYNTH plain G does not preserve note-entry G ownership")

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
for needle in (
    "g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false",
    "if (handled)",
    "g_miniAcid->randomize303Pattern(0);",
    "g_miniAcid->randomize303Pattern(1);",
    "g_miniAcid->randomizeDrumPattern();",
):
    require(SKETCH, needle, f"sketch fallback boundary changed: {needle}")

# The established live bridge and scratch playing path retain the current Stage15
# tonal context/materializer. The generic bridge owns attempt allocation only for
# callers that did not already accept the request in the quantized owner.
for needle in (
    "context.tonalMaterializationEnabled = true;",
    "context.rootPitchClass",
    "context.scaleTypeValue",
    "assignGenerationAttempt(scene.genre, context",
    "migrateStrongRhythmMaterial(",
):
    require(BRIDGE, needle, f"shared Stage 15/reroll route changed: {needle}")

for needle in (
    "context.tonalMaterializationEnabled = true;",
    "context.rootPitchClass",
    "context.scaleTypeValue",
    "context.generationAttemptOrdinal = generationAttemptOrdinal;",
    "migrateStrongRhythmMaterial(",
):
    require(QUANTIZED, needle, f"scratch Stage 15/reroll route changed: {needle}")

print("Release generation routing source regressions: OK")
