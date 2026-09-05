#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

python3 tests/test_p3_u1_source_projection.py

FAST_BUILD_DIR="${ROOT_DIR}/build/pattern-phrase-p3-u1-fast"
mkdir -p "${FAST_BUILD_DIR}"
g++ -std=c++17 -I. tests/test_p3_u1_phrase_projection.cpp \
  -o "${FAST_BUILD_DIR}/test_p3_u1_phrase_projection"
"${FAST_BUILD_DIR}/test_p3_u1_phrase_projection"

bash tests/run_pattern_phrase_p3_tests.sh
