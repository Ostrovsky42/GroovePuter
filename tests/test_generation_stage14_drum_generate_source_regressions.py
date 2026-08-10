#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DRUM_PAGE = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
BRIDGE_H = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.h").read_text(encoding="utf-8")
BRIDGE_CPP = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


require(
    BRIDGE_H,
    "regenerateDrumsWithStrongRhythmMigration",
    "live bridge lacks drum-only strong generation API",
)
require(
    BRIDGE_CPP,
    "engine.randomizeDrumPattern();",
    "drum-only bridge must preserve legacy generation as fallback",
)
require(
    BRIDGE_CPP,
    "return migrateStrongRhythmDrums(",
    "drum-only bridge bypasses strong rhythm materialization",
)
for needle in (
    "context.patternAddress = engine.currentDrumPatternIndex();",
    "context.feelProfile = static_cast<FeelProfileId>(scene.feel.timingProfile);",
    "scene.generatorParams.microTimingAmount",
):
    require(BRIDGE_CPP, needle, f"drum-only bridge missing live context: {needle}")

require(
    DRUM_PAGE,
    "lowerKey == 'g' && !ui_event.ctrl && !ui_event.alt && !ui_event.meta",
    "plain DRUMS G is not explicitly owned by the strong generation route",
)
require(
    DRUM_PAGE,
    "regenerateDrumsWithStrongRhythmMigration",
    "plain DRUMS G still bypasses Genre/Rhythm/Feel materialization",
)

# Voice-local and chaos tools are intentionally not promoted into full relational
# regeneration: they remain explicit destructive/editor operations.
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

print("Stage 14 DRUMS G strong generation source regressions: OK")
