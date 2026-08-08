#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage1_source_regressions.py"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_rhythm_stage1.cpp" \
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp" \
  -o "${BUILD_DIR}/test_rhythm_stage1"
"${BUILD_DIR}/test_rhythm_stage1"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_rhythm_atlas_falsification.cpp" \
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp" \
  -o "${BUILD_DIR}/test_rhythm_atlas_falsification"
"${BUILD_DIR}/test_rhythm_atlas_falsification"
