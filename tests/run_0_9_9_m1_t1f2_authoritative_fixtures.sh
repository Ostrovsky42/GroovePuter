#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)";B="$R/build/host-tests/m1-t1f2";mkdir -p "$B"
python3 "$R/tests/generate_0_9_9_m1_t1f2_pairs.py" "$B/pairs.h"
mapfile -t S < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$R/tests/run_stage15_tonal_integration_tests.sh"|rg '"\$\{ROOT\}/src/'|sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build(){ local c="$1" o="$2";shift 2;"$c" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -I"$R" -I"$B" "$@" "${S[@]/#/$R}" "$R/tests/test_0_9_9_m1_t1f2_authoritative_fixtures.cpp" -o "$o";}
build "${CXX:-g++}" "$B/gcc";"$B/gcc">"$B/gcc1";"$B/gcc">"$B/gcc2";diff -u "$B/gcc1" "$B/gcc2"
if command -v clang++>/dev/null;then build clang++ "$B/clang";"$B/clang">"$B/clang.out";diff -u "$B/gcc1" "$B/clang.out";fi
build "${CXX:-g++}" "$B/san" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined;ASAN_OPTIONS=detect_leaks=0 "$B/san">/dev/null
cat "$B/gcc1"
