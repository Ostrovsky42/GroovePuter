from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE_H = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.h"
).read_text(encoding="utf-8")
BRIDGE_CPP = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
DRUM_PAGE = (
    ROOT / "src/ui/pages/drum_sequencer_page.cpp"
).read_text(encoding="utf-8")
DRUM_LEGACY = (
    ROOT / "src/ui/pages/drum_sequencer_page_legacy.h"
).read_text(encoding="utf-8")
PRODUCTION = (
    ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
).read_text(encoding="utf-8")
REFERENCE = (
    ROOT / "src/generation/rhythm/reference_vocabulary.cpp"
).read_text(encoding="utf-8")
SDL_MAKEFILE = (ROOT / "platform_sdl/Makefile").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# Cardputer ADV has no dedicated Shift key in this workflow. Ctrl+Alt+G owns
# the explicit audition path and recognizes scancode-only G. Plain G, Ctrl+G,
# Alt+G and Ctrl+Alt+G must remain disjoint. R9 keeps plain-G Strong Rhythm
# semantics but routes the persistent result through the canonical bounded
# generation COMMIT so the same one-slot receipt can be toggled by Ctrl+Z.
for needle in (
    "lowerKey == 'g' || ui_event.scancode == GROOVEPUTER_G",
    "if (keyG && ui_event.ctrl && ui_event.alt && !ui_event.meta)",
    "regeneratePhraseAuditionWithProbe",
    '"AUD %uB %s %s #%u"',
    "if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)",
    "regenerateDrumsWithQuantizedCommit",
):
    require(DRUM_PAGE, needle, f"Stage 12 audition input contract changed: {needle}")

# Audition must start from pattern mode and restore the pre-audition pattern
# selection after the bridge temporarily visits reserved Bank B slots.
for needle in (
    'UI::showToast("AUD: EXIT SONG", 1400);',
    "const int previousDrumBank = page->mini_acid_.currentDrumBankIndex();",
    "const int previousDrumPattern = page->mini_acid_.currentDrumPatternIndex();",
    "const int previousSynthBankA = page->mini_acid_.current303BankIndex(0);",
    "const int previousSynthBankB = page->mini_acid_.current303BankIndex(1);",
    "const int previousSynthPatternA = page->mini_acid_.current303PatternIndex(0);",
    "const int previousSynthPatternB = page->mini_acid_.current303PatternIndex(1);",
    "page->mini_acid_.setSongMode(false);",
    "page->mini_acid_.setDrumBankIndex(previousDrumBank);",
    "page->mini_acid_.setDrumPatternIndex(previousDrumPattern);",
    "page->mini_acid_.set303BankIndex(0, previousSynthBankA);",
    "page->mini_acid_.set303BankIndex(1, previousSynthBankB);",
    "page->mini_acid_.set303PatternIndex(0, previousSynthPatternA);",
    "page->mini_acid_.set303PatternIndex(1, previousSynthPatternB);",
    "page->mini_acid_.setSongMode(true);",
):
    require(DRUM_PAGE, needle, f"audition return-state contract changed: {needle}")

require(
    DRUM_LEGACY,
    "mini_acid_.randomizeDrumVoice(voice);",
    "Ctrl+G voice-local randomize contract disappeared",
)
require(
    DRUM_LEGACY,
    "mini_acid_.randomizeDrumPatternChaos();",
    "Alt+G chaos randomize contract disappeared",
)

# Audition is deliberately destructive only inside current-page Bank B + Song B.
for needle in (
    "constexpr int kPhraseAuditionBank = 1;",
    "constexpr int kPhraseAuditionSongSlot = 1;",
    "songPatternFromPageBankIndex",
    "manager.editDrumPatternSet(bar)",
    "manager.editSynthPattern(0, bar)",
    "manager.editSynthPattern(1, bar)",
    "engine.setActiveSongSlot(kPhraseAuditionSongSlot);",
    "engine.setSongPlaybackSlot(kPhraseAuditionSongSlot);",
    "engine.setSongMode(true);",
    "engine.setLoopRange(0, result.requestedBars - 1);",
):
    require(BRIDGE_CPP, needle, f"audition storage/transport contract changed: {needle}")

# Stage 15 remains the tonal owner inside the restored audition path, and the
# one shared request P-level is carried into both phrase evolution and evolved
# drum materialization.
for needle in (
    "context.tonalMaterializationEnabled = true;",
    "context.rootPitchClass",
    "context.scaleTypeValue",
    "migrateStrongRhythmMaterial(",
    "result.level = baseContext.level;",
    "request.level = baseContext.level;",
    "context.level,",
):
    require(BRIDGE_CPP, needle, f"Stage 15 audition context changed: {needle}")

if BRIDGE_CPP.count("engine.sceneManager().editCurrentSynthPattern(0)") != 1:
    raise AssertionError("normal Synth A live binding is no longer unique")
if BRIDGE_CPP.count("engine.sceneManager().editCurrentSynthPattern(1)") != 1:
    raise AssertionError("normal Synth B live binding is no longer unique")

# One production identity is selected and locked before writing bars.
for needle in (
    "migrateStrongRhythmDrums(",
    "RhythmSelectionMode::Manual",
    "lockedSettings.rhythmArchetypeId = result.archetypeId;",
):
    require(BRIDGE_CPP, needle, f"audition identity-lock contract changed: {needle}")

# Multi-bar Stage 12 remains opt-in here only; unsupported one-bar identities
# receive deterministic strong variations instead of being mislabeled evolved.
for needle in (
    "ReferenceVocabulary::phraseEvolutionEnabled",
    "ReferenceVocabulary::phraseEvolutionCatalog()",
    "evolveMultiBarPhrase(request)",
    "AppliedEvolved",
    "AppliedVariationFallback",
    "auditionSeed",
    "0x41554449u",
):
    require(BRIDGE_CPP, needle, f"audition evolution/fallback contract changed: {needle}")

require(
    SDL_MAKEFILE,
    "../src/generation/rhythm/reference_phrase_vocabulary.cpp",
    "SDL/WASM target does not link the Stage 12 reference phrase catalog",
)

# Physical ESP32-S3 probe remains explicit and bounded to Cardputer builds.
for needle in (
    "uxTaskGetStackHighWaterMark(nullptr)",
    "heap_caps_get_free_size(caps)",
    "heap_caps_get_largest_free_block(caps)",
    "maxReductionDurationUs",
    "maxBreakDurationUs",
    '"[PHRASE-PROBE] status=%s level=%s',
):
    require(BRIDGE_CPP, needle, f"Cardputer phrase probe contract changed: {needle}")

for archetype_id in (404, 413, 414, 415, 417, 418, 420, 712, 714):
    require(
        BRIDGE_CPP,
        str(archetype_id),
        f"physical subtractive probe lost archetype {archetype_id}",
    )

# Normal production remains one bar. The explicit audition bridge is the only
# production-linked path allowed to mention the phrase-evolution catalog.
require(
    PRODUCTION,
    "request.phraseBars = 1;",
    "normal strong migration escaped the one-bar production guard",
)
require(
    REFERENCE,
    "value.allowedPhraseBars = phraseBarsBit(1);",
    "normal ReferenceVocabulary was widened by the audition branch",
)
for forbidden in ("phraseEvolutionCatalog", "evolveMultiBarPhrase"):
    if forbidden in PRODUCTION:
        raise AssertionError(
            f"normal strong migration leaked audition-only symbol: {forbidden}"
        )

for needle in (
    "Explicit Stage 12 audition/probe command. It never replaces normal G.",
    "reserves Bank B (current page) patterns 1..8 and Song B for audition",
    "uses the current session P1/P2/P3 request level for selection/evolution",
    "uses the current Stage 15 tonal materialization context for synth A/B",
    "PhraseAuditionProbe",
    "regeneratePhraseAuditionWithProbe",
):
    require(BRIDGE_H, needle, f"audition public contract changed: {needle}")

print("Generation Stage 12 audition/probe source regressions: OK")
