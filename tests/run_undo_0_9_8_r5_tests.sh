#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# R5 is closure on top of the accepted R2/R3/R4 ownership contract.
bash "$ROOT/tests/run_undo_0_9_8_r4_tests.sh"
python3 "$ROOT/tests/test_undo_0_9_8_r5_mutation_closure.py"

echo "0.9.8 R5 mutation ownership closure: PASS"
