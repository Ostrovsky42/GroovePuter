#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i3"
mkdir -p "${BUILD_DIR}"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)
SOURCES+=("/src/generation/migration/phrase_execution.cpp")

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_i3_phrase_law_execution.cpp" \
  -o "${BUILD_DIR}/phrase_law"
"${BUILD_DIR}/phrase_law"

# The phrase law must not leak multi-bar ownership into the shared one-bar
# migration; Stage 12 already guards that boundary.
bash "${ROOT}/tests/run_phrase_stage12_tests.sh" > /dev/null
echo "GF2-I3 Stage 12 ownership boundary: intact"

bash "${ROOT}/tests/run_gf2_i2a_tests.sh"
