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
genre_manager = read("src/dsp/genre_manager.cpp")
ui_input = read("src/ui/ui_input.h")

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
    "streak_ >= 10",
    "streak_ >= 4",
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
    "ARROWS:TARGET",
):
    assert token in generation, f"Generation feedback/navigation contract missing: {token}"

for token in (
    "UIInput::HoldAccelerator hold_accel_",
    "int target_row_ = 0",
    "void onEnter(int context) override",
):
    assert token in generation_header, f"Generation target state missing: {token}"

assert "setSongPosition" not in generation.split("void GenerationPage::moveTargetRow", 1)[1].split("void GenerationPage::materializeCurrentBar", 1)[0], (
    "Browsing Generation targets must remain UI-only until materialization"
)

print("Axis hardware feedback source regressions: PASS")
