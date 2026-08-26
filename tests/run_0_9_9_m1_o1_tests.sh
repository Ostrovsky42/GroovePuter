#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)";B="$R/build/host-tests/m1-o1";mkdir -p "$B"
mapfile -t S < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$R/tests/run_stage15_tonal_integration_tests.sh"|grep -F '"${ROOT}/src/'|sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build(){ local c="$1" o="$2" t="$3"; shift 3; "$c" "$@" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -DGROOVEPUTER_M1_TEST_PROBE -I"$R" "${S[@]/#/$R}" "$R/tests/$t" -o "$o";}
build "${CXX:-g++}" "$B/gcc" test_0_9_9_m1_o1_melodic_request_probe.cpp;"$B/gcc"
if command -v clang++>/dev/null;then build clang++ "$B/clang" test_0_9_9_m1_o1_melodic_request_probe.cpp;"$B/clang";fi
build "${CXX:-g++}" "$B/san" test_0_9_9_m1_o1_melodic_request_probe.cpp -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined;ASAN_OPTIONS=detect_leaks=0 "$B/san"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -I"$R" "${S[@]/#/$R}" "$R/tests/test_0_9_9_m1_o1_noninterference.cpp" -o "$B/off"
build "${CXX:-g++}" "$B/on" test_0_9_9_m1_o1_noninterference.cpp
"$B/off" > "$B/off.txt"; "$B/on" > "$B/on.txt"; diff -u "$B/off.txt" "$B/on.txt"
if nm -C "$B/off" | grep -Eq 'StrongRhythmMelodicRequestProbe|setStrongRhythmMelodicRequestProbe'; then exit 1; fi
nm -C "$B/on" | grep -E 'setStrongRhythmMelodicRequestProbe' >/dev/null
