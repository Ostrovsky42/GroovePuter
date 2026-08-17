#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

bash tests/run_undo_0_9_8_r7_tests.sh
python3 tests/test_undo_0_9_8_r8_global_ctrl_z_source.py

echo "0.9.8 R8 final Safe Editing software contracts: PASS"
