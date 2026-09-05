#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

python3 tests/test_p3_u1_source_projection.py
bash tests/run_pattern_phrase_p3_tests.sh
