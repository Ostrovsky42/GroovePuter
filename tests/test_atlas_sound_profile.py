#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")

start = source.index("void GenreManager::applyGenreTimbre")
end = source.index("void GenreManager::applyTexture", start)
block = source[start:end]

required = (
    "if (state_.recipe == 6)",
    'engine.setSynthEngine(0, "TB303")',
    'engine.setSynthEngine(1, "TB303")',
)
for item in required:
    if item not in block:
        raise AssertionError(f"missing Chicago Jack sound-profile invariant: {item}")

engine_switch = block.index('engine.setSynthEngine(0, "TB303")')
parameter_write = block.index("engine.set303ParameterNormalized")
if engine_switch >= parameter_write:
    raise AssertionError("TB303 engine must be selected before parameter writes")

print("Atlas sound profile regression: OK")
