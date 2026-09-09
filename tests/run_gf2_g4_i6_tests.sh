#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-i6"
mkdir -p "$BUILD"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${ROOT}" \
  "${ROOT}/tests/test_gf2_g4_i6_ownership_census.cpp" \
  -o "$BUILD/g4-i6-ownership-census"

"$BUILD/g4-i6-ownership-census" | tee "$BUILD/g4-i6-summary.txt"
