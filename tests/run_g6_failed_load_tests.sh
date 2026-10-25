#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bash "$ROOT/tests/run_cardputer_prune_tests.sh"
cd "$ROOT/platform_sdl"
COMMON=(-std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME -DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0)
"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_failed_load_preserves_scene.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-failed-load
../build/cardputer-prune-host/g6-failed-load
../build/cardputer-prune-host/g6-failed-load crc
printf '%s\n' 'G6 staged Scene refusal, corrupt header and CRC: PASS'

"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_failed_melody_load.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-failed-melody-load
for damage in header crc missing nonselected oversized; do
  ../build/cardputer-prune-host/g6-failed-melody-load "$damage"
done
printf '%s\n' 'G6 resident Melody refusal (header, CRC, missing, nonselected, oversized): PASS'

cd "$ROOT"
"${CXX:-g++}" -std=c++17 -Wno-c++20-extensions -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h tests/test_g6_melody_read_size.cpp -o build/cardputer-prune-host/g6-melody-read-size
build/cardputer-prune-host/g6-melody-read-size
