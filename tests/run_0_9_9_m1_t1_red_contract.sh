#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)";B="$R/build/host-tests/m1-t1";mkdir -p "$B"
python3 "$R/tests/test_0_9_9_m1_t1_source_guard.py"
mapfile -t S < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$R/tests/run_stage15_tonal_integration_tests.sh"|rg '"\$\{ROOT\}/src/'|sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build(){ local c="$1" o="$2";shift 2;"$c" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -I"$R" "$@" "${S[@]/#/$R}" "$R/tests/test_0_9_9_m1_t1_red_contract.cpp" -o "$o";}
build "${CXX:-g++}" "$B/gcc";"$B/gcc" --baseline|tee "$B/gcc1";"$B/gcc" --baseline|tee "$B/gcc2";diff -u "$B/gcc1" "$B/gcc2"
if command -v clang++>/dev/null;then build clang++ "$B/clang";"$B/clang" --baseline|tee "$B/clang.out";diff -u "$B/gcc1" "$B/clang.out";fi
build "${CXX:-g++}" "$B/san" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined;ASAN_OPTIONS=detect_leaks=0 "$B/san" --baseline
"$B/gcc"|tee "$B/green";rg -q 'M1_T1_ADDRESS_INVARIANCE PASS' "$B/green";rg -q 'M1_T1_ONE_SELECTION PASS' "$B/green";echo M1_T1_OVERALL GREEN
