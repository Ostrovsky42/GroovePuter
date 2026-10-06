#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/h0-r1"
mkdir -p "$BUILD"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/tests/test_h0_r1_phrase_harmonic_timeline.cpp" \
  "$ROOT/src/generation/generation_context.cpp" \
  "$ROOT/src/generation/roles/chord_progression.cpp" \
  -o "$BUILD/timeline"
"$BUILD/timeline"
