#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
mode_source = (ROOT / "src/dsp/mode_manager.cpp").read_text(encoding="utf-8")
genre_source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")
filter_source = (ROOT / "src/dsp/filter.cpp").read_text(encoding="utf-8")


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

# The former audibility clamps lived only in the dead applyGenreTimbre API and
# had no callers. Genre metadata must not regain physical synth patch ownership;
# audible TB303 safety remains enforced in the actual DSP path below.
for projection in (
    "applyGenreTimbre",
    "setSynthEngine(",
    "set303ParameterNormalized",
):
    forbid(genre_source, projection,
           f"Genre code must not project the Synth A patch: {projection}")

chamberlin_start = filter_source.index("float ChamberlinFilter::process")
chamberlin_end = filter_source.index("// === DIODE FILTER", chamberlin_start)
chamberlin_process = filter_source[chamberlin_start:chamberlin_end]

require(
    filter_source,
    "fastSinForChamberlin",
    "Synth A TB303 low-pass path must keep the non-transcendental Chamberlin coefficient",
)
require(
    chamberlin_process,
    "fastSinForChamberlin(phase)",
    "Chamberlin process must use the fast coefficient approximation",
)
forbid(
    chamberlin_process,
    "sinf(",
    "Chamberlin process must not call sinf per audio sample",
)

print("Synth A bass profile source regressions: OK")
