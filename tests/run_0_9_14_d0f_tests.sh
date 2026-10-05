#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/0-9-14-d0f"
D0E_HEAD="919d6db1b1951f94813958459ca68767268cb5ef"
mkdir -p "$BUILD"

cd "$ROOT"

if ! git cat-file -e "${D0E_HEAD}^{commit}" 2>/dev/null; then
  git fetch --no-tags --depth=1 origin "$D0E_HEAD"
fi


echo "== D0-E regression gate =="
bash "$ROOT/tests/run_0_9_14_d0e_tests.sh"

echo "== D0-F P0 preservation contract =="
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I"$ROOT" -I"$ROOT/platform_sdl" \
  -include "$ROOT/platform_sdl/arduino_compat.h" \
  "$ROOT/tests/test_0_9_14_d0f_p0_preservation_contract.cpp" \
  -o "$BUILD/p0-preservation"
"$BUILD/p0-preservation"

echo "== D0-F source seam audit =="
python3 "$ROOT/tests/test_0_9_14_d0f_source_regressions.py"


echo "0.9.14 D0-F: GREEN"