#!/usr/bin/env bash
# 0.9.19 S1: synth engine switches never free or swap an engine in the audio
# thread; type and object agree through and after a crossfade (ASan + UBSan).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/s1"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"
cd "$ROOT"

SRCS=(
  src/dsp/swappable_synth_voice.cpp
  src/dsp/mini_tb303.cpp
  src/dsp/filter.cpp
  src/dsp/audio_wavetables.cpp
  src/dsp/ay_synth_voice.cpp
  src/dsp/sh101_synth_voice.cpp
  src/dsp/sid_synth.cpp
  src/dsp/sid_synth_voice.cpp
  src/dsp/sn76489_synth_voice.cpp
  src/dsp/wave_morph_synth_voice.cpp
)
"$CXX" -std=c++17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -g \
  -I. -include platform_sdl/arduino_compat.h \
  "${SRCS[@]}" tests/test_s1_synth_engine_switch.cpp \
  -o "$BUILD/test_s1_synth_engine_switch"
"$BUILD/test_s1_synth_engine_switch"
python3 tests/test_s1_synth_engine_switch_source_regressions.py
