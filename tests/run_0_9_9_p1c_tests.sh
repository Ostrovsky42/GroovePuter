#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "== 0.9.9-P1c source contracts =="
python3 tests/test_0_9_9_p1c_effective_pattern_ref_source.py

echo "== 0.9.9-P1c MiniAcid runtime contract =="
make -C platform_sdl clean p1c-effective-pattern-ref-test CXX=g++

echo "== cumulative P1 ownership contracts =="
bash tests/run_0_9_9_p1b_tests.sh

echo "0.9.9-P1c focused + cumulative contracts passed"
