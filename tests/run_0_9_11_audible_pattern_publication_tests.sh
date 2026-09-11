#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I"${ROOT}" \
  "${ROOT}/tests/test_0_9_11_audible_pattern_publication.cpp" \
  -o "${BUILD_DIR}/test_0_9_11_audible_pattern_publication"
"${BUILD_DIR}/test_0_9_11_audible_pattern_publication"

# Preserve the pre-existing generation ownership and Pattern/Phrase runtime gates.
bash "${ROOT}/tests/run_generation_0_9_9_c_tests.sh"
python3 "${ROOT}/tests/test_pattern_phrase_p2_source_contract.py"
python3 "${ROOT}/tests/test_synth_persistence_source_regressions.py"

printf '%s\n' '0.9.11 audible Pattern publication focused gate: PASS'
