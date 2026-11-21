#!/usr/bin/env python3
"""Regenerate the GF2-I2A FEEL amplitude census artifact.

Compiles the amplitude dump against the real production generation sources and
writes the measured TSV. The artifact is the evidence the I2A amplitude decision
is taken on, and the regression surface that keeps it honest afterwards.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "host-tests" / "gf2-i2a"
OUT = BUILD / "gf2_i2a_feel_amplitude_dump"
ARTIFACT = ROOT / "docs/research/GF2_I2A_FEEL_AMPLITUDE_CENSUS.tsv"

STAGE15_RUNNER = ROOT / "tests/run_stage15_tonal_integration_tests.sh"
HGS_SOURCE_LIST = ROOT / "tests/hgs_song_state_sources.sh"


def common_sources() -> list[str]:
    """Reuse the authoritative Stage 15 source list rather than a second copy."""
    lines = STAGE15_RUNNER.read_text(encoding="utf-8").splitlines()
    collecting = False
    sources: list[str] = []
    for line in lines:
        if line.strip().startswith("COMMON_SOURCES=("):
            collecting = True
            continue
        if collecting:
            if line.strip() == ")":
                break
            token = line.strip().strip('"')
            if token.startswith("${ROOT}/"):
                sources.append(token[len("${ROOT}/"):])
    if not sources:
        raise RuntimeError("could not read COMMON_SOURCES from the Stage 15 runner")
    return sources


def hgs_song_state_sources() -> list[str]:
    """Reuse the shared Song Harmonic State dependency list used by host tests."""
    result = subprocess.run(
        [
            "bash",
            "-c",
            'ROOT_DIR="$1"; source "$2"; printf "%s\\n" "${HGS_SONG_STATE_SOURCES[@]}"',
            "gf2-i2a-source-list",
            str(ROOT),
            str(HGS_SOURCE_LIST),
        ],
        check=True,
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    return [
        Path(source).resolve().relative_to(ROOT).as_posix()
        for source in result.stdout.splitlines()
        if source
    ]


SOURCES = [
    *common_sources(),
    *hgs_song_state_sources(),
    "src/dsp/genre_manager.cpp",
    "scenes.cpp",
    "json_evented.cpp",
    "src/audio/pattern_paging.cpp",
    "tools/gf2/gf2_i2a_feel_amplitude_dump.cpp",
]


def compile_dump() -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wvla",
            "-Wno-c++20-extensions",
            "-Wno-unused-variable",
            "-Wno-unused-but-set-variable",
            f"-I{ROOT}",
            f"-I{ROOT / 'platform_sdl'}",
            "-include",
            str(ROOT / "platform_sdl/arduino_compat.h"),
            *[str(ROOT / source) for source in SOURCES],
            "-o",
            str(OUT),
        ],
        check=True,
        cwd=ROOT,
    )


def main() -> int:
    compile_dump()
    census = subprocess.run(
        [str(OUT)], check=True, cwd=ROOT, text=True, capture_output=True
    ).stdout
    rows = census.splitlines()
    if len(rows) < 2:
        raise RuntimeError("amplitude census produced no rows")
    if "NOT_APPLIED" in census:
        raise RuntimeError(
            "a genre/recipe failed to materialize; the census must cover the "
            "whole production corpus"
        )
    ARTIFACT.write_text(census, encoding="utf-8")
    print(f"GF2-I2A amplitude census: {len(rows) - 1} rows -> {ARTIFACT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
