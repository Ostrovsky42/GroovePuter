#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)";B="$R/build/host-tests/m1-t1f";mkdir -p "$B"
mapfile -t S < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$R/tests/run_stage15_tonal_integration_tests.sh"|rg '"\$\{ROOT\}/src/'|sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build(){ local c="$1" o="$2";shift 2;"$c" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -I"$R" "$@" "${S[@]/#/$R}" "$R/tests/test_0_9_9_m1_t1f_fixture_reachability.cpp" -o "$o";}
build "${CXX:-g++}" "$B/gcc";"$B/gcc">"$B/gcc1";"$B/gcc">"$B/gcc2";diff -u "$B/gcc1" "$B/gcc2";diff -u "$R/tests/data/m1_t1f_physical_fixture_reachability.txt" "$B/gcc1"
if command -v clang++>/dev/null;then build clang++ "$B/clang";"$B/clang">"$B/clang.out";diff -u "$B/gcc1" "$B/clang.out";fi
build "${CXX:-g++}" "$B/san" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined;ASAN_OPTIONS=detect_leaks=0 "$B/san">/dev/null
set +e;bash "$R/tests/run_0_9_9_m1_t1_red_contract.sh">"$B/red" 2>&1;x=$?;set -e;test "$x" -ne 0;rg -q 'M1_T1_ADDRESS_INVARIANCE RED' "$B/red";rg -q 'M1_T1_ONE_SELECTION RED' "$B/red";echo M1_T1F_T1_GCC_RED_CONFIRMED;echo M1_T1F_T2_GCC_RED_CONFIRMED
