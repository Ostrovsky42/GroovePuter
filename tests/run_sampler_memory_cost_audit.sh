#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_sampler_memory_ownership_source.py"

# scenes.h uses default member initializers on bit-fields. GCC diagnoses those
# as a C++20 feature under -Werror in a strict C++17 host probe, even though the
# production embedded toolchain accepts the source. Use GNU++20 only for this
# ABI/layout measurement executable; this does not change firmware language
# settings or production behavior.
"${CXX}" \
  -std=gnu++20 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_memory_layout.cpp" \
  -o "${BUILD_DIR}/test_sampler_memory_layout"

"${BUILD_DIR}/test_sampler_memory_layout"

echo "sampler memory cost audit passed"
