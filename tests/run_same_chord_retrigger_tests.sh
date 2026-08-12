#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT}/tests/test_same_chord_retrigger_source_regressions.py"

SOURCES=(
  "${ROOT}/src/generation/roles/chord_rhythm_retrigger.cpp"
  "${ROOT}/tests/test_same_chord_retrigger.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -I"${ROOT}" "$@" "${SOURCES[@]}" -o "${output}"
  "${output}"
}

"${CXX:-g++}" -std=c++17 -Os -Wall -Wextra -Werror -Wvla -fstack-usage \
  -I"${ROOT}" -c "${ROOT}/src/generation/roles/chord_rhythm_retrigger.cpp" \
  -o "${BUILD_DIR}/chord_rhythm_retrigger_stack.o"
STACK_FILE="${BUILD_DIR}/chord_rhythm_retrigger_stack.su"
STACK_BYTES="$(grep 'realizeChordRhythmRetriggers' "${STACK_FILE}" | head -n1 | awk -F '\t' '{print $2}')"
test -n "${STACK_BYTES}"
test "${STACK_BYTES}" -le 192
echo "P3 retrigger stack usage (-Os host): ${STACK_BYTES} B (gate 192 B)"

build_and_run "${CXX:-g++}" "${BUILD_DIR}/same_chord_retrigger_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/same_chord_retrigger_clang"
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/same_chord_retrigger_sanitize" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

echo "P3 same-chord retrigger host gate: OK"
