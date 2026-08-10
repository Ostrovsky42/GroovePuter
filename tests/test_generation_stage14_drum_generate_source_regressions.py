#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DRUM_PAGE = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_LEGACY = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
BRIDGE_H = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.h").read_text(encoding="utf-8")
BRIDGE_CPP = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text(encoding="utf-8")
MINI_DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
SETTINGS_ALIAS = (ROOT / "pages/settings_page.h").read_text(encoding="utf-8")
FEEL_PAGE_H = (ROOT / "src/ui/pages/feel_page.h").read_text(encoding="utf-8")
FEEL_PAGE_CPP = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")


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

# Cardputer key events may carry only the hardware scancode. The strong plain-G
# owner must match the same G representations as the retained legacy handler;
# otherwise hardware falls through to randomizeDrumPattern().
require(
    DRUM_PAGE,
    "lowerKey == 'g' || ui_event.scancode == GROOVEPUTER_G",
    "plain DRUMS G strong route ignores the Cardputer G scancode",
)
require(
    DRUM_PAGE,
    "if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)",
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

# The production Generate workflow normalizes legacy page ids 8/11 to page 9.
# Page 9 is still constructed under the historical SettingsPage factory name,
# whose compatibility alias must resolve to the real Stage 14 FeelPage. Pin the
# complete route so a green test cannot validate controls in an unreachable page.
for needle in (
    "constexpr int kTexture = 8",
    "constexpr int kFeel = 9",
    "constexpr int kGeneration = 11",
    "if (page == kTexture || page == kGeneration) return kFeel;",
):
    require(WORKFLOW, needle, f"production FEEL workflow route changed: {needle}")
require(
    MINI_DISPLAY,
    "case 9:  page = std::make_unique<SettingsPage>(gfx_, mini_acid_, audio_guard_); break;",
    "production page 9 factory route changed unexpectedly",
)
require(
    SETTINGS_ALIAS,
    "using SettingsPage = FeelPage;",
    "production page 9 no longer resolves to Stage 14 FeelPage",
)

for needle in (
    "Profile = 0",
    "Repeats,",
):
    require(FEEL_PAGE_H, needle, f"Stage 14 FEEL focus model missing: {needle}")
for needle in (
    '"PROFILE"',
    '"SWING OFFBEAT"',
    '"FEEL AMOUNT"',
    '"VELOCITY VAR"',
    '"REPEATS"',
    '"PRESET"',
    "scene.feel.timingProfile = next;",
    "scene.feel.patternBars = next;",
):
    require(FEEL_PAGE_CPP, needle, f"Stage 14 FEEL runtime control missing: {needle}")

print("Stage 14 DRUMS G / production FEEL route source regressions: OK")
