#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/tests/test_atomic_file_commit.cpp" -o "$ROOT/build/cardputer-prune-host/g6-atomic-file-commit"
"$ROOT/build/cardputer-prune-host/g6-atomic-file-commit"
printf '%s\n' 'G6 Cardputer pointer backup retry: PASS'
bash "$ROOT/tests/run_cardputer_prune_tests.sh"
cd "$ROOT/platform_sdl"
COMMON=(-std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME -DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0)
"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_failed_load_preserves_scene.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-failed-load
../build/cardputer-prune-host/g6-failed-load
../build/cardputer-prune-host/g6-failed-load crc
printf '%s\n' 'G6 staged Scene refusal, corrupt header and CRC: PASS'

"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_selection_rollback.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-selection-rollback
../build/cardputer-prune-host/g6-selection-rollback

"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_failed_melody_load.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-failed-melody-load
for damage in header crc missing nonselected oversized; do
  ../build/cardputer-prune-host/g6-failed-melody-load "$damage"
done
printf '%s\n' 'G6 resident Melody refusal (header, CRC, missing, nonselected, oversized): PASS'

cd "$ROOT"
"${CXX:-g++}" -std=c++17 -Wno-c++20-extensions -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h tests/test_g6_melody_read_size.cpp -o build/cardputer-prune-host/g6-melody-read-size
build/cardputer-prune-host/g6-melody-read-size

cd "$ROOT/platform_sdl"
"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_late_hydration.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-late-hydration
../build/cardputer-prune-host/g6-late-hydration
../build/cardputer-prune-host/g6-late-hydration prepare-fail
printf '%s\n' 'G6 selected Melody preparation and post-read fault: PASS'

"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_page_metadata.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-page-metadata
../build/cardputer-prune-host/g6-page-metadata
"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_nonresident_load.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-nonresident-load
for damage in header crc missing oversized page-crc page-missing drums-page-missing legacy-json last-slot; do
 ../build/cardputer-prune-host/g6-nonresident-load "$damage"
done
printf '%s\n' 'G6 nonresident material, page faults and legacy JSON: PASS'

"${CXX:-g++}" "${COMMON[@]}" $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 ../tests/test_g6_new_project_melody_reset.cpp ../build/cardputer-prune-host/libgrooveputer_p3.a $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o ../build/cardputer-prune-host/g6-new-project-melody-reset
../build/cardputer-prune-host/g6-new-project-melody-reset
../build/cardputer-prune-host/g6-new-project-melody-reset clear
printf '%s\n' 'G6 New/Clear resets Melody descriptors and reloads blank project: PASS'
