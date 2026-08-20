#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash tests/run_undo_0_9_8_r4_tests.sh
python3 tests/test_undo_0_9_8_r5_ownership_closure.py

echo "0.9.8 R5 ownership closure tests passed"
