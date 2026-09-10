#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u4b5"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -I. tests/test_ui_constitution_u4b5_runtime_phrase_undo.cpp \
  -o "${BUILD_DIR}/test_ui_constitution_u4b5_runtime_phrase_undo"
"${BUILD_DIR}/test_ui_constitution_u4b5_runtime_phrase_undo"
python3 tests/test_ui_constitution_u4b5_runtime_phrase_undo_integration.py
printf '%s\n' 'UI Constitution U4B5 focused gate: PASS'
