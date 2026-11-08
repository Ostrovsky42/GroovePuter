#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/growth-hold-safety"
mkdir -p "$BUILD"

g++ -std=c++17 -I"$ROOT" -I"$ROOT/platform_sdl" \
  -include "$ROOT/platform_sdl/arduino_compat.h" \
  "$ROOT/tests/test_growth_hold_safety.cpp" -o "$BUILD/test"
"$BUILD/test"
printf '%s\n' 'Growth and HOLD safety: PASS'
