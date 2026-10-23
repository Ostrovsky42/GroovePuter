#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Refresh modified DSP objects in the established real-engine pruned-host archive.
bash "$ROOT/tests/run_cardputer_prune_tests.sh"
cd "$ROOT/platform_sdl"
COMMON=(-std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME -DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0)
"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_dsp_a_project_roundtrip.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/dsp-a/project-roundtrip
../build/dsp-a/project-roundtrip
