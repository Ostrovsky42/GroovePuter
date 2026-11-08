#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/melody-store"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -Wno-c++20-extensions -I. tests/test_melody_store_format.cpp \
  -o "${BUILD_DIR}/test"
"${BUILD_DIR}/test"
printf '%s\n' 'M2a melody store format gate: PASS'
