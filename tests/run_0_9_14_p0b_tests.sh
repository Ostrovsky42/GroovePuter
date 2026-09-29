#!/usr/bin/env bash
# P0-B1 gate: bass/chord follow the bar function; frozen bar functions stay bit-identical.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/0-9-14-p0b"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"
cd "$ROOT"

echo "== P0-B1 role transforms (pure, exhaustive properties) =="
"$CXX" -std=c++17 -Wall -Wextra -I"$ROOT" tests/test_0_9_14_p0b_bar_function_roles.cpp -o "$BUILD/p0b_roles"
"$BUILD/p0b_roles"

echo "== P0-B1 source regressions =="
python3 tests/test_0_9_14_p0b_source_regressions.py

echo "== P0-B1 golden compatibility (every lane of 1384 bars vs the pre-B1 golden) =="
M0_P0_BUILD_ONLY=1 bash tools/m0/build_m0a_p0.sh "$BUILD/tool" >/dev/null
M0_P0_GOLDEN=1 M0_P0_OUT="$BUILD/tool" "$BUILD/tool/m0a_p0" > "$BUILD/golden_run.txt"
python3 tests/test_0_9_14_p0b_golden_compare.py tests/golden/p0b_bar_function_golden_pre_b1.tsv "$BUILD/tool/p0b_golden.tsv"
echo "== P0-B2 admission compatibility (baseline: golden after B1, before any admission) =="
python3 tests/test_0_9_14_p0b2_golden_compare.py tests/golden/p0b_bar_function_golden_b1.tsv "$BUILD/tool/p0b_golden.tsv"
echo "== P0-B2 step 2 compatibility (baseline: golden after steppers) =="
python3 tests/test_0_9_14_p0b2_golden_compare.py tests/golden/p0b_bar_function_golden_b2_steppers.tsv "$BUILD/tool/p0b_golden.tsv"
rm -rf "$ROOT/patterns" "$ROOT/platform_sdl/patterns" "$ROOT/projects" "$ROOT/grooveputer_scene_name.txt"

echo "0.9.14 P0-B (B1 + B2 steps): GREEN"
