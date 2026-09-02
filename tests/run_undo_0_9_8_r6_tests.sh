#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash tests/run_undo_0_9_8_r5_tests.sh
python3 tests/test_undo_0_9_8_r6_ux_source_regressions.py

echo "0.9.8 R6 Undo UX tests passed"
