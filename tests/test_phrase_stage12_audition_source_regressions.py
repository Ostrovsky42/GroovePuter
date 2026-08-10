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


# Cardputer ADV has no dedicated Shift key in this workflow. Ctrl+Alt+G owns the
# explicit audition path and still recognizes scancode-only G. Plain G, Ctrl+G,
# and Alt+G must remain disjoint.
for needle in (
    "lowerKey == 'g' || ui_event.scancode == GROOVEPUTER_G",
    "if (keyG && ui_event.ctrl && ui_event.alt && !ui_event.meta)",
    "regeneratePhraseAuditionWithProbe",
    '"AUD %uB %s #%u"',
    "if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)",
    "regenerateDrumsWithStrongRhythmMigration",
):
    require(DRUM_PAGE, needle, f"Stage 12 audition input contract changed: {needle}")

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

# Audition is deliberately destructive only inside current-page Bank B + Song B
# and uses explicitly addressed current-bank local slots. The normal current-
# pattern synth bindings must remain unique to the ordinary whole-pattern G path.
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

if BRIDGE_CPP.count("engine.sceneManager().editCurrentSynthPattern(0)") != 1:
    raise AssertionError("normal Synth A live binding is no longer unique")
if BRIDGE_CPP.count("engine.sceneManager().editCurrentSynthPattern(1)") != 1:
    raise AssertionError("normal Synth B live binding is no longer unique")

# One production Stage 14 identity is selected and locked before writing bars.
for needle in (
    "migrateStrongRhythmDrums(",
    "RhythmSelectionMode::Manual",
    "lockedSettings.rhythmArchetypeId = result.archetypeId;",
    "migrateStrongRhythmMaterial(",
):
    require(BRIDGE_CPP, needle, f"audition identity-lock contract changed: {needle}")

# Multi-bar Stage 12 is opt-in here only. Unsupported one-bar archetypes still
# get deterministic per-bar strong variations so every supported genre can be
# auditioned without pretending that fallback variation is phrase evolution.
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

# SDL/WASM compile the same live bridge, so the candidate catalog implementation
# must be linked there as well. This pins the exact build-boundary regression
# that previously produced undefined references to phraseEvolutionEnabled() and
# phraseEvolutionCatalog().
require(
    SDL_MAKEFILE,
    "../src/generation/rhythm/reference_phrase_vocabulary.cpp",
    "SDL/WASM target does not link the Stage 12 reference phrase catalog",
)

# Physical ESP32-S3 probe: execute the actual linked candidate catalog and record
# stack lifetime minimum, internal heap, largest free block, and worst
# Reduction/Break duration.
for needle in (
    "uxTaskGetStackHighWaterMark(nullptr)",
    "heap_caps_get_free_size(caps)",
    "heap_caps_get_largest_free_block(caps)",
    "maxReductionDurationUs",
    "maxBreakDurationUs",
    '"[PHRASE-PROBE] status=%s',
):
    require(BRIDGE_CPP, needle, f"Cardputer phrase probe contract changed: {needle}")

for archetype_id in (404, 413, 414, 415, 417, 418, 420, 712, 714):
    require(
        BRIDGE_CPP,
        str(archetype_id),
        f"physical subtractive probe lost archetype {archetype_id}",
    )

# Normal production is still one-bar. The explicit audition bridge is the only
# new production-linked caller allowed to mention phraseEvolutionCatalog.
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
    "PhraseAuditionProbe",
    "regeneratePhraseAuditionWithProbe",
):
    require(BRIDGE_H, needle, f"audition public contract changed: {needle}")

print("Generation Stage 12 audition/probe source regressions: OK")
