#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

cd "${ROOT_DIR}"

# Keep the existing P4I picker/lease contract green first.
bash tests/run_0_9_9_p4i_tests.sh

mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  tests/test_0_9_9_p4i_01_song_alt_routing.cpp \
  -o "${BUILD_DIR}/test_0_9_9_p4i_01_song_alt_routing"

"${BUILD_DIR}/test_0_9_9_p4i_01_song_alt_routing"
python3 tests/test_0_9_9_p4i_01_song_alt_routing_source.py

echo "0.9.9-P4I-01 focused + cumulative P4I suite: PASS"
