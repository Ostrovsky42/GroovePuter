#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/performance-macro-engine"
CXX="${CXX:-g++}"
mkdir -p "${BUILD}"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"${ROOT}" \
  "${ROOT}/tests/test_performance_macro_engine.cpp" \
  -o "${BUILD}/test_performance_macro_engine"

"${BUILD}/test_performance_macro_engine"
echo "Performance macro engine tests: PASS"
