#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

cd "${ROOT_DIR}"

bash tests/run_0_9_9_p1a2_tests.sh

mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  tests/test_0_9_9_p4i_pattern_picker_lease.cpp \
  -o "${BUILD_DIR}/test_0_9_9_p4i_pattern_picker_lease"

"${BUILD_DIR}/test_0_9_9_p4i_pattern_picker_lease"
python3 tests/test_0_9_9_p4i_pattern_picker_source.py

echo "0.9.9-P4I focused + cumulative lease suite: PASS"
