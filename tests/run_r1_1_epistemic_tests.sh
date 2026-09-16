#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BUILD="$ROOT/build/host-tests/r1_1"; mkdir -p "$BUILD"
g++ -std=c++17 -Wno-c++20-extensions -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h \
  tests/test_r1_1_epistemic.cpp -o "$BUILD/test"
"$BUILD/test"
