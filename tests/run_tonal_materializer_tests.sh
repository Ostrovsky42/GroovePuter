#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT}/tests/test_tonal_materializer_source_regressions.py"

SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/src/generation/tonal/tonal_projector.cpp"
  "${ROOT}/src/generation/tonal/tonal_materializer.cpp"
  "${ROOT}/tests/test_tonal_materializer.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -I"${ROOT}" "$@" "${SOURCES[@]}" -o "${output}"
  "${output}"
}

# Fixed-capacity generation is not enough if a later edit silently grows the
# call frame. Measure the owner under size optimization to match the firmware
# build policy rather than reporting an unoptimized host-only frame size.
"${CXX:-g++}" -std=c++17 -Os -Wall -Wextra -Werror -Wvla -fstack-usage \
  -I"${ROOT}" -c "${ROOT}/src/generation/tonal/tonal_materializer.cpp" \
  -o "${BUILD_DIR}/tonal_materializer_stack.o"
STACK_FILE="${BUILD_DIR}/tonal_materializer_stack.su"
STACK_BYTES="$(grep 'materializeTonalIntent' "${STACK_FILE}" | head -n1 | awk -F '\t' '{print $2}')"
test -n "${STACK_BYTES}"
test "${STACK_BYTES}" -le 384
echo "Tonal Materializer stack usage (-Os): ${STACK_BYTES} B (gate 384 B)"

build_and_run "${CXX:-g++}" "${BUILD_DIR}/tonal_materializer_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/tonal_materializer_clang"
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/tonal_materializer_sanitize" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

echo "Stage 15 Tonal Materializer gate: OK"
