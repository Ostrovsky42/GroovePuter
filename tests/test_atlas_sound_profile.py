#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")

start = source.index("void GenreManager::applyGenreTimbre")
end = source.index("void GenreManager::applyTexture", start)
block = source[start:end]

required = (
    "state_.recipe == 6 || state_.recipe == 7",
    "state_.recipe >= 8 && state_.recipe <= 11",
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

texture_start = source.index("void GenreManager::applyTexture")
texture_end = source.index("// STRUCTURAL BEHAVIOR", texture_start)
texture = source[texture_start:texture_end]
if texture.count('currentSynthEngineName(voice) == "TB303"') < 2:
    raise AssertionError("texture biases must not write TB303 parameters into OPL2/SID/AY")

print("Atlas sound profile regression: OK")
