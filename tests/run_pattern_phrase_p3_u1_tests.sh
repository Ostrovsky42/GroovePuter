#!/bin/sh
set -eu
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"
python3 tests/test_p3_u1_source_projection.py
sh tests/run_pattern_phrase_p3_tests.sh
