#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

bash tests/run_undo_0_9_8_r6_tests.sh
python3 tests/test_undo_0_9_8_r7_acceptance_source.py

echo "0.9.8 R7 ADV acceptance software contracts: PASS"
