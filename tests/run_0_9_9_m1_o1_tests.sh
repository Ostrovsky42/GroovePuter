#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)";B="$R/build/host-tests/m1-o1";mkdir -p "$B"
mapfile -t S < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$R/tests/run_stage15_tonal_integration_tests.sh"|rg '"\$\{ROOT\}/src/'|sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build(){ "$1" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -DGROOVEPUTER_M1_TEST_PROBE -I"$R" "${S[@]/#/$R}" "$R/tests/test_0_9_9_m1_o1_melodic_request_probe.cpp" -o "$2";}
build "${CXX:-g++}" "$B/gcc";"$B/gcc"
if command -v clang++>/dev/null;then build clang++ "$B/clang";"$B/clang";fi
ASAN_OPTIONS=detect_leaks=0 build "${CXX:-g++}" "$B/san" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined;ASAN_OPTIONS=detect_leaks=0 "$B/san"
