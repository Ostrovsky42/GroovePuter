#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_sampler_memory_ownership_source.py"

# scenes.h uses default member initializers on bit-fields. GCC diagnoses those
# as a C++20 feature under -Werror in a strict C++17 host probe, even though the
# production embedded toolchain accepts the source. Use GNU++20 only for these
# host measurement executables; this does not change firmware language settings
# or production behavior.
"${CXX}" \
  -std=gnu++20 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_memory_layout.cpp" \
  -o "${BUILD_DIR}/test_sampler_memory_layout"

"${BUILD_DIR}/test_sampler_memory_layout"

"${CXX}" \
  -std=gnu++20 \
  -Wall \
  -Wextra \
  -Werror \
  -pthread \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_memory_dynamic_cost.cpp" \
  "${ROOT_DIR}/src/sampler/sample_index.cpp" \
  "${ROOT_DIR}/src/sampler/ram_sample_store.cpp" \
  "${ROOT_DIR}/src/sampler/sample_loader.cpp" \
  -o "${BUILD_DIR}/test_sampler_memory_dynamic_cost"

"${BUILD_DIR}/test_sampler_memory_dynamic_cost"

echo "sampler memory cost audit passed"
