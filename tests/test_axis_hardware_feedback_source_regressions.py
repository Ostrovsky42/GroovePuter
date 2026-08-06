#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

song = read("src/ui/pages/song_page.cpp")
feel = read("src/ui/pages/feel_page.cpp")
texture = read("src/ui/pages/texture_page.cpp")
generation = read("src/ui/pages/generation_page.cpp")
generation_header = read("src/ui/pages/generation_page.h")
genre = read("src/ui/pages/genre_page.cpp")
tb303 = read("src/ui/pages/tb303_params_page.cpp")
drum_automation = read("src/ui/pages/drum_automation_page.cpp")
genre_manager = read("src/dsp/genre_manager.cpp")
ui_input = read("src/ui/ui_input.h")
status_chrome = read("src/ui/ui_status_chrome.h")
layout_manager = read("src/ui/layout_manager.cpp")

for token in (
    "AtlasRuntime::hasRecipe(activeRecipe)",
    "AtlasRuntime::applyRecipe(activeRecipe",
    "GenreManager::grooveboxModeForRecipe",
    "generator.setFlavorLocal(0)",
    "genreTag * 17u + recipeTag * 5u",
):
    assert token in song, f"Song genre materialization contract missing: {token}"

for token in (
    "tape.fxEnabled = tapeOn;",
    "currentScene().feel.tapeEnabled = tapeOn;",
):
    assert token in genre_manager, f"Texture audible path missing: {token}"

for token in (
    "applyTexture(false);",
    "LIVE / ENTER REAPPLY",
    "AUDIBLE TAPE",
    "HOLD L/R:ACCEL",
):
    assert token in texture, f"Texture feedback contract missing: {token}"

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
    "GEN BLOCKED ROW",
    "LAST %s",
    "CURRENT EMPTY SONG ROW",
    "generator.setFlavorLocal(0)",
    "UIInput::navCode(event)",
    "moveTargetRow(-1",
    "moveTargetRow(1",
    "hold_accel_.multiplier",
    "L/R:+-1  U/D:+-8",
    "target_row_ + (nav == GROOVEPUTER_DOWN ? 8 : -8)",
    "[GENERATION] target %d -> %d",
    "GEN TARGET ROW %d",
    "[GENERATION] write request row=%d",
):
    assert token in generation, f"Generation feedback/navigation contract missing: {token}"

for token in (
    "UIInput::HoldAccelerator hold_accel_",
    "int target_row_ = 0",
    "void onEnter(int context) override",
):
    assert token in generation_header, f"Generation target state missing: {token}"

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

assert "setSongPosition" not in generation.split("void GenerationPage::moveTargetRow", 1)[1].split("void GenerationPage::materializeCurrentBar", 1)[0], (
    "Browsing Generation targets must remain UI-only until materialization"
)

print("Axis hardware feedback source regressions: PASS")
