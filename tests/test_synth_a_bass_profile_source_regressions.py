#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
mode_source = (ROOT / "src/dsp/mode_manager.cpp").read_text(encoding="utf-8")
genre_source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")


def require(source: str, needle: str, message: str) -> None:
    if needle not in source:
        raise AssertionError(message)


def forbid(source: str, needle: str, message: str) -> None:
    if needle in source:
        raise AssertionError(message)


require(
    mode_source,
    "baseRoot = std::max(0, std::min(params.minOctave, 127));",
    "Synth A must derive its bass floor from the genre MIDI-note range",
)
require(
    mode_source,
    "bassMaxNote = std::min(requestedMax, baseRoot + 12);",
    "Synth A must remain bounded to one octave above the genre floor",
)
require(
    mode_source,
    "if (isBass) note = std::max(baseRoot, std::min(note, bassMaxNote));",
    "Synth A generated notes must be clamped to the bass range",
)
forbid(
    mode_source,
    "baseRoot = 24; // C1",
    "Synth A must not force every genre to C1",
)
forbid(
    mode_source,
    "params.minOctave > 0 && params.minOctave < 36",
    "MIDI-note range must not be treated as an octave index or ignored at 36+",
)

for bound in (
    "if (cut < 0.18f) cut = 0.18f;",
    "if (cut > 0.62f) cut = 0.62f;",
    "if (env < 0.18f) env = 0.18f;",
    "if (env > 0.55f) env = 0.55f;",
    "if (decay < 0.10f) decay = 0.10f;",
    "if (decay > 0.45f) decay = 0.45f;",
):
    require(genre_source, bound, f"missing Synth A audibility bound: {bound}")

for old_bound in (
    "if (cut < 0.05f) cut = 0.05f;",
    "if (cut > 0.45f) cut = 0.45f;",
    "if (env < 0.02f) env = 0.02f;",
    "if (env > 0.20f) env = 0.20f;",
    "if (decay < 0.04f) decay = 0.04f;",
    "if (decay > 0.25f) decay = 0.25f;",
):
    forbid(genre_source, old_bound, f"obsolete over-dark Synth A bound remains: {old_bound}")

print("Synth A bass profile source regressions: OK")
