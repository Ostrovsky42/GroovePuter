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
  "${ROOT_DIR}/tests/test_sampler_persistence_ownership.cpp" \
  "${ROOT_DIR}/src/sampler/sample_index.cpp" \
  "${ROOT_DIR}/src/sampler/sample_scene_persistence.cpp" \
  -o "${BUILD_DIR}/test_sampler_persistence_ownership"

"${BUILD_DIR}/test_sampler_persistence_ownership"
python3 "${ROOT_DIR}/tests/test_sampler_persistence_source_regressions.py"

echo "sampler persistence ownership tests passed"
