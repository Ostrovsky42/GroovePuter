#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/dual-osc-preset-ui"
mkdir -p "$BUILD"
DEFAULT_SOURCES="$(make -C "$ROOT/platform_sdl" -pn 2>/dev/null | sed -n 's/^SOURCES := //p')"
SOURCES=" ${DEFAULT_SOURCES} "
SOURCES="${SOURCES// sdl_main.cpp / }"
"${CC:-gcc}" -std=c11 -O1 -I"$ROOT" -c "$ROOT/src/dsp/felucca_analog_c.c" -o "$BUILD/felucca_analog_c.o"
cd "$ROOT/platform_sdl"
# Compile the C shim as C, then enable the experimental adapter consistently
# in every C++ translation unit. Standard SDL/default engine flags stay unchanged.
# shellcheck disable=SC2086
"${CXX:-g++}" -std=c++17 -Wno-c++20-extensions -O1 -I.. -I. -include arduino_compat.h \
  -DUSE_RETRO_THEME -DUSE_AMBER_THEME -DGROOVEPUTER_ENABLE_SAMPLER=0 \
  -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0 \
  -DGROOVEPUTER_FELUCCA_ANALOG=1 $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) \
  $SOURCES ../src/dsp/felucca_analog_voice.cpp "$BUILD/felucca_analog_c.o" \
  ../src/sampler/sample_scene_persistence.cpp ../tests/test_dual_osc_preset_ui.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$BUILD/test"
"$BUILD/test"
