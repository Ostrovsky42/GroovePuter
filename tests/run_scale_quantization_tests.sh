#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/grooveputer-scale-quantization"
mkdir -p "$BUILD_DIR"

python3 "$ROOT/tests/test_scale_quantization_source_regressions.py"

SOURCES=(
  "$ROOT/src/dsp/advanced_pattern_generator.cpp"
  "$ROOT/tests/test_scale_quantization.cpp"
)

COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -I"$ROOT"
)

g++ "${COMMON_FLAGS[@]}" "${SOURCES[@]}" -o "$BUILD_DIR/scale_quantization_gcc"
"$BUILD_DIR/scale_quantization_gcc"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${COMMON_FLAGS[@]}" "${SOURCES[@]}" -o "$BUILD_DIR/scale_quantization_clang"
  "$BUILD_DIR/scale_quantization_clang"
fi

g++ "${COMMON_FLAGS[@]}" \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  "${SOURCES[@]}" \
  -o "$BUILD_DIR/scale_quantization_sanitize"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD_DIR/scale_quantization_sanitize"

echo "Scale quantization gate: OK"
