#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/0-9-14-d0d1"
D0C_HEAD="89635069f15bb116410eb540db6bd147f8f808e5"
D0D1_HEAD="5696567ca7d540c8683115aa5a5e20e2f9c48596"
mkdir -p "$BUILD"

cd "$ROOT"

if ! git cat-file -e "${D0C_HEAD}^{commit}" 2>/dev/null; then
  git fetch --no-tags --depth=1 origin "$D0C_HEAD"
fi

if ! git cat-file -e "${D0D1_HEAD}^{commit}" 2>/dev/null; then
  git fetch --no-tags --depth=1 origin "$D0D1_HEAD"
fi

echo "== D0-D1 bounded production delta =="
mapfile -t SRC_DELTA < <(git diff --name-only "$D0C_HEAD" "$D0D1_HEAD" -- src/ | sort)
EXPECTED=(
  "src/dsp/development_semantic_adapter.h"
  "src/dsp/development_semantics.h"
)
mapfile -t EXPECTED_SORTED < <(printf '%s\n' "${EXPECTED[@]}" | sort)
if [[ "${SRC_DELTA[*]}" != "${EXPECTED_SORTED[*]}" ]]; then
  echo "D0-D1 ERROR: unexpected production delta"
  printf 'actual:   %s\n' "${SRC_DELTA[*]}"
  printf 'expected: %s\n' "${EXPECTED_SORTED[*]}"
  exit 2
fi

echo "== D0-C regression gate =="
bash "$ROOT/tests/run_0_9_14_d0c_tests.sh"

echo "== D0-D1 runtime witnesses =="
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I"$ROOT" -I"$ROOT/platform_sdl" \
  -include "$ROOT/platform_sdl/arduino_compat.h" \
  "$ROOT/tests/test_0_9_14_d0d1_claim_strength.cpp" \
  -o "$BUILD/claim-strength"
"$BUILD/claim-strength"

echo "== D0-D1 source firewall =="
python3 "$ROOT/tests/test_0_9_14_d0d1_source_regressions.py"

echo "0.9.14 D0-D1: GREEN"