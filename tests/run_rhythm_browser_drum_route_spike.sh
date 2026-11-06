#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests"
mkdir -p "${BUILD}"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"${ROOT}" "${ROOT}/tests/test_rhythm_browser_drum_route_spike.cpp" \
  -o "${BUILD}/test_rhythm_browser_drum_route_spike"
"${BUILD}/test_rhythm_browser_drum_route_spike"
