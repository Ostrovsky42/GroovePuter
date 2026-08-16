#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
CPP = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")
SCENES = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

EXPECTED_MODE_LINES = [
    "Acid = 0",
    "Outrun = 1",
    "Darksynth = 2",
    "Electro = 3",
    "Rave = 4",
    "Reggae = 5",
    "TripHop = 6",
    "Broken = 7",
    "Chip = 8",
    "House = 9",
    "Techno = 10",
    "HipHop = 11",
    "FunkSoul = 12",
    "UkGarage = 13",
    "DrumAndBass = 14",
    "LoFi = 15",
]
for line in EXPECTED_MODE_LINES:
    assert line in HEADER, f"persisted GenerativeMode identity changed: {line}"

EXPECTED_LOFI_RECIPES = [
    ("kClassicChillRecipeId", 12, "Classic Chill"),
    ("kDrunkenGrooveRecipeId", 13, "Drunken Groove"),
    ("kLoFiHouseRecipeId", 14, "Lo-Fi House"),
    ("kMinimalSleepRecipeId", 15, "Minimal Sleep"),
    ("kGoldenEraRecipeId", 16, "Golden Era"),
    ("kDustyJazzRecipeId", 17, "Dusty Jazz"),
]
for symbol, value, name in EXPECTED_LOFI_RECIPES:
    assert f"{symbol} = {value}" in HEADER, f"Lo-Fi recipe ID changed: {symbol}"
    assert f'{{{symbol}, "{name}", {{}}' in CPP, f"Lo-Fi semantic recipe changed: {name}"

# Lo-Fi is a semantic identity. Its current low-level parameter substrate may
# share TripHop defaults; compatibility must not rename/re-number Lo-Fi because
# of that implementation detail.
assert "case GenerativeMode::LoFi" in CPP
assert "kPresetTripHop" in CPP

# Scene persistence must save and restore the semantic mode and recipe bytes.
assert 'genreObj["gen"] = scene_->genre.generativeMode;' in SCENES
assert 'genreObj["rcp"] = scene_->genre.recipe;' in SCENES
assert 'loaded->genre.generativeMode = static_cast<uint8_t>(gen);' in SCENES
assert 'loaded->genre.recipe = static_cast<uint8_t>(recipe);' in SCENES

# Loading old projects must not resurrect the removed TEXTURE owner.
assert "Legacy TEXTURE values are accepted but intentionally ignored." in SCENES

print("0.9.9 generation compatibility source regressions passed")
