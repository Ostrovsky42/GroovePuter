#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/grooveputer-logging-macro-compat"
mkdir -p "$OUT"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/tests/test_logging_macro_compat.cpp" -o "$OUT/test"
"$OUT/test"
echo "Logging macro compatibility: PASS"
