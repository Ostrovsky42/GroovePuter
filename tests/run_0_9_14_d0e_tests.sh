#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/0-9-14-d0e"
D0D1_HEAD="5696567ca7d540c8683115aa5a5e20e2f9c48596"
mkdir -p "$BUILD"

cd "$ROOT"

if ! git cat-file -e "${D0D1_HEAD}^{commit}" 2>/dev/null; then
  git fetch --no-tags --depth=1 origin "$D0D1_HEAD"
fi

echo "== D0-E research-only source boundary =="
if ! git diff --exit-code "$D0D1_HEAD" -- src/; then
  echo "D0-E ERROR: production src delta is forbidden"
  exit 2
fi

echo "== D0-D1 regression gate =="
bash "$ROOT/tests/run_0_9_14_d0d1_tests.sh"

echo "== D0-E provenance/invalidation research model =="
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I"$ROOT" -I"$ROOT/platform_sdl" \
  -include "$ROOT/platform_sdl/arduino_compat.h" \
  "$ROOT/tests/test_0_9_14_d0e_provenance_contract.cpp" \
  -o "$BUILD/provenance-contract"
"$BUILD/provenance-contract"

echo "== D0-E source evidence/firewall =="
python3 "$ROOT/tests/test_0_9_14_d0e_source_regressions.py"

if ! git diff --exit-code "$D0D1_HEAD" -- src/; then
  echo "D0-E ERROR: source changed during checkpoint"
  exit 2
fi

echo "0.9.14 D0-E: GREEN"