#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sample_ref.cpp" \
  "${ROOT_DIR}/src/sampler/sample_index.cpp" \
  -o "${BUILD_DIR}/test_sample_ref"

"${BUILD_DIR}/test_sample_ref"

echo "sampler stable SampleRef tests passed"
