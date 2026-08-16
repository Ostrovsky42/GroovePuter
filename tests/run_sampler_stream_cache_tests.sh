#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/.build/sampler_stream_cache"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_sampler_streaming_source_regressions.py"

g++ -std=gnu++20 -Wall -Wextra -Werror -pthread \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_stream_cache.cpp" \
  "${ROOT_DIR}/src/sampler/ram_sample_store.cpp" \
  "${ROOT_DIR}/src/sampler/sample_loader.cpp" \
  "${ROOT_DIR}/src/sampler/sample_index.cpp" \
  -o "${BUILD_DIR}/test_sampler_stream_cache"

"${BUILD_DIR}/test_sampler_stream_cache"

echo "sampler stream cache tests: PASS"
