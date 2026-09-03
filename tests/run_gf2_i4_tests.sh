#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i4"
mkdir -p "${BUILD_DIR}"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_i4_corridor_consumers.cpp" \
  -o "${BUILD_DIR}/corridor_consumers"
"${BUILD_DIR}/corridor_consumers"

PHRASE_SOURCES=("${SOURCES[@]}")
PHRASE_SOURCES+=("/src/generation/migration/phrase_execution.cpp")
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${PHRASE_SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_i4_phrase_density.cpp" \
  -o "${BUILD_DIR}/phrase_density"
"${BUILD_DIR}/phrase_density"

python3 "${ROOT}/tests/test_gf2_i4_request_initializer_contract.py"

# Explicit one-bar compatibility proof for the pre-I4 RhythmRealizer contract.
bash "${ROOT}/tests/run_rhythm_stage2_tests.sh" > /dev/null
echo "GF2-I4 one-bar RhythmRealizer compatibility: PASS"

# Run inherited GF2 checkpoints explicitly. Some runners intentionally chain to
# earlier gates as well; explicit invocation here makes the I4 evidence legible.
bash "${ROOT}/tests/run_gf2_i1_tests.sh" > /dev/null
echo "GF2-I1 tempo/corridor arbitration regression: PASS"
bash "${ROOT}/tests/run_gf2_i2_tests.sh" > /dev/null
echo "GF2-I2 profile FEEL regression: PASS"
bash "${ROOT}/tests/run_gf2_i2a_tests.sh" > /dev/null
echo "GF2-I2A FEEL amplitude regression: PASS"
bash "${ROOT}/tests/run_gf2_i3_tests.sh" > /dev/null
echo "GF2-I3 phrase-law execution regression: PASS"

echo "GF2-I4 focused + inherited regression gate: PASS"
