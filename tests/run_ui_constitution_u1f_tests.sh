#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CXX="${CXX:-g++}"
OUT="/tmp/grooveputer-ui-u1f-shell-frame"

"$CXX" -std=c++17 -Wall -Wextra -Werror \
  tests/test_ui_constitution_u1f_shell_frame_model.cpp \
  -o "$OUT"
"$OUT"
python3 tests/test_ui_constitution_u1f_shell_pixel_ownership.py

echo "U1F shell pixel ownership PASS"
