#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

cd "${ROOT_DIR}"

# IUI1 composes the complete ancestry stack before its focused proof.
bash tests/run_0_9_9_p4i_01_tests.sh

mkdir -p "${BUILD_DIR}"
"${CXX}" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  -I"${ROOT_DIR}/platform_sdl" \
  tests/test_0_9_9_iui1_picker_lease_integration.cpp \
  src/audio/pattern_paging.cpp \
  -o "${BUILD_DIR}/test_0_9_9_iui1_picker_lease_integration"

"${BUILD_DIR}/test_0_9_9_iui1_picker_lease_integration"
python3 tests/test_0_9_9_iui1_picker_lease_source.py

echo "0.9.9-IUI1 focused + cumulative integration suite: PASS"
