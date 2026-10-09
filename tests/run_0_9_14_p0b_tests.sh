#!/usr/bin/env bash
# P0-B gate: bass/chord follow the bar function (properties); every bar matches the 0.9.18 golden.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/0-9-14-p0b"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"
cd "$ROOT"
source "$ROOT/tests/lib/isolated_workdir.sh"
isolated_init

echo "== P0-B1 role transforms (pure, exhaustive properties) =="
"$CXX" -std=c++17 -Wall -Wextra -I"$ROOT" tests/test_0_9_14_p0b_bar_function_roles.cpp -o "$BUILD/p0b_roles"
"$BUILD/p0b_roles"

echo "== P0-B1 source regressions =="
python3 tests/test_0_9_14_p0b_source_regressions.py

echo "== P0-B golden (every lane of every bar vs the 0.9.18 golden) =="
M0_P0_BUILD_ONLY=1 bash tools/m0/build_m0a_p0.sh "$BUILD/tool" >/dev/null
M0_P0_GOLDEN=1 M0_P0_OUT="$BUILD/tool" "$BUILD/tool/m0a_p0" > "$BUILD/golden_run.txt"
if [[ "${M0_P0B_REBASELINE:-0}" == "1" ]]; then
  cp "$BUILD/tool/p0b_golden.tsv" tests/golden/p0b_bar_function_golden_0918.tsv
  echo "re-baselined tests/golden/p0b_bar_function_golden_0918.tsv"
fi
# The pre-B1, B1 and B2-steppers goldens are kept as history; 0.9.18 changed generation on purpose.
python3 tests/test_0918_p0b_golden_compare.py tests/golden/p0b_bar_function_golden_0918.tsv "$BUILD/tool/p0b_golden.tsv"

echo "0.9.14 P0-B (properties + 0.9.18 golden): GREEN"
