#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i1"
mkdir -p "${BUILD_DIR}"

# Reuse the authoritative Stage 15 source list so the corridor owner and the
# migration pipeline under test are the real production translation units.
mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

build() {
  local compiler="$1"
  local output="$2"
  shift 2
  # The generated Atlas corpus reader keeps the warning profile of its own
  # authoritative host test (tests/run_host_tests.sh); it is compiled apart so
  # the I1 sources can stay on -Werror.
  "${compiler}" "$@" -std=c++17 -Wall -Wextra -I"${ROOT}" -c \
    "${ROOT}/src/dsp/atlas_runtime.cpp" -o "${output}.atlas_runtime.o"
  "${compiler}" "$@" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT}" \
    "${COMMON_SOURCES[@]/#/${ROOT}}" \
    "${output}.atlas_runtime.o" \
    "${ROOT}/tests/test_gf2_i1_tempo_corridor_arbitration.cpp" \
    -o "${output}"
}

build "${CXX:-g++}" "${BUILD_DIR}/tempo_corridor_arbitration"
"${BUILD_DIR}/tempo_corridor_arbitration"

python3 "${ROOT}/tests/test_source_regressions.py"
