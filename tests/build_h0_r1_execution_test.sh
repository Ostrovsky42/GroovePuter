#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST="${1:?test source required}"
OUTPUT="${2:?output required}"
mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "$ROOT/tests/run_stage15_tonal_integration_tests.sh" |
  sed -nE 's/.*"\$\{ROOT\}(\/src\/.*)".*/\1/p'
)
RESOLVED=()
for source in "${SOURCES[@]}"; do RESOLVED+=("$ROOT$source"); done
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable -I"$ROOT" \
  "${RESOLVED[@]}" "$ROOT/src/generation/migration/phrase_execution.cpp" \
  "$ROOT/$TEST" -o "$OUTPUT" "${@:3}"
