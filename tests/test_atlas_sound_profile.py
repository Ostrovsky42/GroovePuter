#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")

start = source.index("void GenreSceneView::applyGenreTimbre")
end = source.index("void GenreSceneView::applyTexture", start)
block = source[start:end]

required = (
    "recipe() == 6 || recipe() == 7",
    "recipe() >= 8 && recipe() <= 11",
    'engine.setSynthEngine(0, "TB303")',
    'engine.setSynthEngine(1, "TB303")',
    'engine.setSynthEngine(1, "OPL2")',
    'engine.currentSynthEngineName(v) != "TB303"',
)
for item in required:
    if item not in block:
        raise AssertionError(f"missing Atlas sound-profile invariant: {item}")

engine_switch = block.index('engine.setSynthEngine(0, "TB303")')
parameter_write = block.index("engine.set303ParameterNormalized")
if engine_switch >= parameter_write:
    raise AssertionError("preview engines must be selected before parameter writes")

texture_start = source.index("void GenreSceneView::applyTexture")
texture = source[texture_start:]
if texture.count('currentSynthEngineName(voice) == "TB303"') < 2:
    raise AssertionError("texture biases must not write TB303 parameters into OPL2/SID/AY")

if "state_." in source:
    raise AssertionError("genre runtime must not retain a second mutable state owner")

print("Atlas sound profile regression: OK")
