#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/0-9-14-d0c"
D0B_HEAD="d3c1e41a9613b83d4c6f579a284d6df4b37f4522"
mkdir -p "$BUILD"

cd "$ROOT"

echo "== D0-C production-delta boundary =="
mapfile -t SRC_DELTA < <(git diff --name-only "$D0B_HEAD" -- src/ | sort)
EXPECTED=(
  "src/dsp/development_semantic_adapter.h"
  "src/dsp/development_semantics.h"
)
mapfile -t EXPECTED_SORTED < <(printf '%s\n' "${EXPECTED[@]}" | sort)

if [[ "${SRC_DELTA[*]}" != "${EXPECTED_SORTED[*]}" ]]; then
  echo "D0-C ERROR: unexpected production delta"
  printf 'actual:   %s\n' "${SRC_DELTA[*]}"
  printf 'expected: %s\n' "${EXPECTED_SORTED[*]}"
  exit 2
fi

echo "== D0-C semantic adapter runtime witnesses =="
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I"$ROOT" -I"$ROOT/platform_sdl" \
  -include "$ROOT/platform_sdl/arduino_compat.h" \
  "$ROOT/tests/test_0_9_14_d0c_semantic_adapter.cpp" \
  -o "$BUILD/semantic-adapter"

"$BUILD/semantic-adapter"

echo "== D0-C source firewall =="
python3 "$ROOT/tests/test_0_9_14_d0c_source_regressions.py"

echo "0.9.14 D0-C: GREEN"
