#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i2"
mkdir -p "${BUILD_DIR}"

# Reuse the authoritative Stage 15 source list so the FEEL owners under test are
# the real production translation units.
mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${COMMON_SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_i2_profile_feel_contract.cpp" \
  -o "${BUILD_DIR}/profile_feel_contract"
"${BUILD_DIR}/profile_feel_contract"

# The generated Atlas corpus reader keeps the warning profile of its own
# authoritative host test (tests/run_host_tests.sh); it is compiled apart so
# the I2 sources can stay on -Werror.
"${CXX:-g++}" -std=c++17 -Wall -Wextra -I"${ROOT}" -c \
  "${ROOT}/src/dsp/atlas_runtime.cpp" -o "${BUILD_DIR}/atlas_runtime.o"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${COMMON_SOURCES[@]/#/${ROOT}}" \
  "${BUILD_DIR}/atlas_runtime.o" \
  "${ROOT}/tests/test_gf2_i2_atlas_swing_ownership.cpp" \
  -o "${BUILD_DIR}/atlas_swing_ownership"
"${BUILD_DIR}/atlas_swing_ownership"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable \
  -Wno-unused-but-set-variable -Wno-c++20-extensions \
  -I"${ROOT}" -I"${ROOT}/platform_sdl" \
  -include "${ROOT}/platform_sdl/arduino_compat.h" \
  "${ROOT}/tests/test_gf2_i2_feel_persistence.cpp" \
  "${ROOT}/scenes.cpp" "${ROOT}/json_evented.cpp" \
  "${ROOT}/src/audio/pattern_paging.cpp" \
  -o "${BUILD_DIR}/feel_persistence"
"${BUILD_DIR}/feel_persistence"
