#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/grooveputer-tonal-projector"
mkdir -p "$BUILD_DIR"

python3 "$ROOT/tests/test_tonal_projector_source_regressions.py"

SOURCES=(
  "$ROOT/src/generation/tonal/tonal_projector.cpp"
  "$ROOT/tests/test_tonal_projector.cpp"
)

COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -I"$ROOT"
)

g++ "${COMMON_FLAGS[@]}" "${SOURCES[@]}" -o "$BUILD_DIR/tonal_projector_gcc"
"$BUILD_DIR/tonal_projector_gcc"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${COMMON_FLAGS[@]}" "${SOURCES[@]}" -o "$BUILD_DIR/tonal_projector_clang"
  "$BUILD_DIR/tonal_projector_clang"
fi

g++ "${COMMON_FLAGS[@]}" \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  "${SOURCES[@]}" \
  -o "$BUILD_DIR/tonal_projector_sanitize"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD_DIR/tonal_projector_sanitize"

echo "Tonal Projector host matrix: OK"
