#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u4b8"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -I. tests/test_ui_constitution_u4b8_phrase_insert_edit.cpp \
  -o "${BUILD_DIR}/test_u4b8"
"${BUILD_DIR}/test_u4b8"
printf '%s\n' 'UI Constitution U4B8 focused gate: PASS'
