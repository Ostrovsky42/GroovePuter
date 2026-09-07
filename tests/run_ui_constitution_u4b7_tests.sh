#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u4b7"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -I. tests/test_ui_constitution_u4b7_phrase_pitch_edit.cpp \
  -o "${BUILD_DIR}/test_ui_constitution_u4b7_phrase_pitch_edit"
"${BUILD_DIR}/test_ui_constitution_u4b7_phrase_pitch_edit"
printf '%s\n' 'UI Constitution U4B7 focused gate: PASS'
