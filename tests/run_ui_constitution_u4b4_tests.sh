#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u4b4"
mkdir -p "${BUILD_DIR}"

c++ -std=c++17 -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_ui_constitution_u4b4_runtime_phrase_undo.cpp" \
  -o "${BUILD_DIR}/test_runtime_phrase_undo"
"${BUILD_DIR}/test_runtime_phrase_undo"

python3 "${ROOT_DIR}/tests/test_ui_constitution_u4b4_runtime_phrase_undo_integration.py"

echo "UI Constitution U4B4 runtime Phrase Undo gate: PASS"
