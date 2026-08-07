#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


song = read("src/ui/pages/song_page.cpp")
feel = read("src/ui/pages/feel_page.cpp")
genre = read("src/ui/pages/genre_page.cpp")
tb303 = read("src/ui/pages/tb303_params_page.cpp")
drum_automation = read("src/ui/pages/drum_automation_page.cpp")
genre_catalog = read("src/dsp/genre_catalog.cpp")
ui_input = read("src/ui/ui_input.h")
status_chrome = read("src/ui/ui_status_chrome.h")
layout_manager = read("src/ui/layout_manager.cpp")

for removed in (
    "src/ui/pages/texture_page.cpp",
    "src/ui/pages/texture_page.h",
    "src/ui/pages/generation_page.cpp",
    "src/ui/pages/generation_page.h",
):
    assert not (ROOT / removed).exists(), f"Retired UI source must not return: {removed}"

for token in (
    "AtlasRuntime::hasRecipe(activeRecipe)",
    "AtlasRuntime::applyRecipe(activeRecipe",
    "GenreSceneView::grooveboxModeForRecipe",
    "generator.setFlavorLocal(0)",
    "genreTag * 17u + recipeTag * 5u",
):
    assert token in song, f"Song genre materialization contract missing: {token}"

# Removing the standalone page must not remove the audible compatibility path
# used by persisted scenes and explicit genre materialization.
for token in (
    "tape.fxEnabled = tapeOn;",
    "currentScene().feel.tapeEnabled = tapeOn;",
):
    assert token in genre_catalog, f"Persisted texture audible path missing: {token}"

for token in (
    "LIVE: offbeat playback delay",
    "NEXT GEN: note timing spread",
    "NEXT GEN: note velocity spread",
    "HOLD L/R:ACCEL",
):
    assert token in feel, f"FEEL causality contract missing: {token}"

for token in (
    "class HoldAccelerator",
    "bool forcedFast = false",
    "if (forcedFast) return 5;",
    "streak_ >= 14",
    "streak_ >= 8",
    "streak_ >= 3",
    "multiplierAt",
):
    assert token in ui_input, f"Hold acceleration missing: {token}"

for token in (
    "static UIInput::HoldAccelerator morphAccelerator",
    "morphAccelerator.multiplier(delta)",
    "adjustMorph(delta * (event.shift || event.ctrl ? 32 : 8) * multiplier)",
):
    assert token in genre, f"Genre MORPH hold acceleration missing: {token}"

for token in (
    "static UIInput::HoldAccelerator knobAccelerator",
    "knobAccelerator.multiplier(1)",
    "knobAccelerator.multiplier(-1)",
    "HOLD:ACCEL [CTRL]FINE",
):
    assert token in tb303, f"Synth parameter hold acceleration missing: {token}"

for token in (
    "static UIInput::HoldAccelerator holdAccelerator",
    "row_ == Row::NodeValue",
    "row_ == Row::GrooveSwing",
    "row_ == Row::GrooveHumanize",
    "delta *= holdAccelerator.multiplier(direction)",
):
    assert token in drum_automation, f"Drum automation hold acceleration missing: {token}"

for token in (
    "uint16_t bpm{uiStatusBpm()}",
    '"%s %s %s %u BPM B%u/%u %s %s%s%s"',
    "lhs.bpm == rhs.bpm",
):
    assert token in status_chrome, f"BPM status chrome contract missing: {token}"

assert "UI::setUiStatusBpm(bpm);" in layout_manager, (
    "Standard header must feed the current BPM into status chrome"
)

print("Axis hardware feedback source regressions: PASS")
