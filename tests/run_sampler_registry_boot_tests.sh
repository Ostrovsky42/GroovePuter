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
  "${ROOT_DIR}/tests/test_sample_registry_boot.cpp" \
  "${ROOT_DIR}/src/sampler/sample_index.cpp" \
  "${ROOT_DIR}/src/sampler/sample_scene_persistence.cpp" \
  "${ROOT_DIR}/src/sampler/ram_sample_store.cpp" \
  -o "${BUILD_DIR}/test_sample_registry_boot"

"${BUILD_DIR}/test_sample_registry_boot"
python3 "${ROOT_DIR}/tests/test_sampler_registry_boot_source_regressions.py"

echo "sampler registry/boot lifecycle tests passed"
