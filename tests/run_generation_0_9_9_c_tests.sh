#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I"${ROOT}" \
  "${ROOT}/tests/test_generation_activation_0_9_9.cpp" \
  -o "${BUILD_DIR}/test_generation_activation_0_9_9"
"${BUILD_DIR}/test_generation_activation_0_9_9"

python3 "${ROOT}/tests/test_generation_activation_0_9_9_source.py"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I"${ROOT}" \
  "${ROOT}/tests/test_generation_undo_owner_0_9_9.cpp" \
  -o "${BUILD_DIR}/test_generation_undo_owner_0_9_9"
"${BUILD_DIR}/test_generation_undo_owner_0_9_9"
python3 "${ROOT}/tests/test_generation_undo_owner_0_9_9_source.py"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I"${ROOT}" \
  "${ROOT}/tests/test_pattern_generation_owner_0_9_9.cpp" \
  -o "${BUILD_DIR}/test_pattern_generation_owner_0_9_9"
"${BUILD_DIR}/test_pattern_generation_owner_0_9_9"
python3 "${ROOT}/tests/test_pattern_generation_owner_0_9_9_source.py"

bash "${ROOT}/tests/run_undo_0_9_8_r7_tests.sh"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT}" \
  "${ROOT}/tests/test_generation_0_9_9_compatibility.cpp" \
  -o "${BUILD_DIR}/test_generation_0_9_9_compatibility"
"${BUILD_DIR}/test_generation_0_9_9_compatibility"
python3 "${ROOT}/tests/test_generation_0_9_9_source_regressions.py"
