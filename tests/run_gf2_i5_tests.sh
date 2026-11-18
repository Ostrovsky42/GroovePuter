#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i5"
# Check the current integration delta. The original I5 ancestor includes
# later accepted documentation with legacy trailing whitespace.
BASE_SHA="$(git -C "${ROOT}" merge-base origin/main HEAD)"
mkdir -p "${BUILD_DIR}"

git -C "${ROOT}" cat-file -e "${BASE_SHA}^{commit}"
git -C "${ROOT}" diff --check "${BASE_SHA}...HEAD"
echo "GF2-I5 git diff --check: PASS"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_i5_depth_role_hierarchy.cpp" \
  -o "${BUILD_DIR}/depth_role_hierarchy"
"${BUILD_DIR}/depth_role_hierarchy"

# I5 must preserve the complete implemented GF2 production-semantic baseline.
bash "${ROOT}/tests/run_gf2_i4_tests.sh" > /dev/null
echo "GF2-I4 musical corridor consumers regression: PASS"

echo "GF2-I5 focused + inherited regression gate: PASS"
