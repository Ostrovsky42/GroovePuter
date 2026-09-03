#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i2a"
mkdir -p "${BUILD_DIR}"

mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

# The magnitude contract reads the shipped GeneratorParams default, so it links
# the same Scene sources production does rather than a stand-in.
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-variable -Wno-unused-but-set-variable \
  -I"${ROOT}" -I"${ROOT}/platform_sdl" \
  -include "${ROOT}/platform_sdl/arduino_compat.h" \
  "${COMMON_SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/src/dsp/genre_manager.cpp" \
  "${ROOT}/scenes.cpp" "${ROOT}/json_evented.cpp" \
  "${ROOT}/src/audio/pattern_paging.cpp" \
  "${ROOT}/tests/test_gf2_i2a_feel_magnitude.cpp" \
  -o "${BUILD_DIR}/feel_magnitude"
"${BUILD_DIR}/feel_magnitude"

# The measured artifact must stay reproducible from the current sources.
python3 "${ROOT}/tools/gf2/generate_gf2_i2a_feel_amplitude_census.py"
git -C "${ROOT}" diff --exit-code -- docs/research/GF2_I2A_FEEL_AMPLITUDE_CENSUS.tsv
echo "GF2-I2A amplitude census artifact: UP TO DATE"

bash "${ROOT}/tests/run_gf2_i2_tests.sh"
