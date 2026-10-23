#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/build/memory-prune-a-delay-host"
ARCHIVE="$ROOT/build/cardputer-prune-host/libgrooveputer_p3.a"
[[ -f "$ARCHIVE" ]] || { echo 'Run bash tests/run_cardputer_prune_tests.sh first' >&2; exit 2; }
mkdir -p "$BUILD"
cd "$ROOT/platform_sdl"
COMMON=(-std=c++17 -Wno-c++20-extensions -O1 -I.. -I. -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME -DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0)
read -ra SDL_FLAGS <<< "$(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx)"
read -ra SDL_LIBS <<< "$(sdl2-config --libs) $(pkg-config --libs SDL2_gfx)"
for BYPASS in 0 1; do
  "${CXX:-g++}" "${COMMON[@]}" "${SDL_FLAGS[@]}" -DGROOVEPUTER_EXPERIMENT_NO_TEMPO_DELAY="$BYPASS" -c ../src/dsp/miniacid_engine.cpp -o "$BUILD/engine-$BYPASS.o"
  "${CXX:-g++}" "${COMMON[@]}" "${SDL_FLAGS[@]}" -DGROOVEPUTER_EXPERIMENT_NO_TEMPO_DELAY="$BYPASS" ../tests/test_tempo_delay_prune_experiment.cpp "$BUILD/engine-$BYPASS.o" "$ARCHIVE" "${SDL_LIBS[@]}" -o "$BUILD/impulse-$BYPASS"
  "$BUILD/impulse-$BYPASS"
done
