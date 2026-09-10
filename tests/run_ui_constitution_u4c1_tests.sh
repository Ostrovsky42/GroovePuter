#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u4c1"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -I. tests/test_ui_constitution_u4c1_onset_navigation.cpp \
  -o "${BUILD_DIR}/test_u4c1"
"${BUILD_DIR}/test_u4c1"
printf '%s\n' 'UI Constitution U4C1 focused gate: PASS'
