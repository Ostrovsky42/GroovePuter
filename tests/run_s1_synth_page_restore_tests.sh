#!/usr/bin/env bash
# 0.9.19 S1: Synth page tab restore before components exist (real UI + engine).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/s1"
mkdir -p "$BUILD"
bash "$ROOT/tools/perf/build.sh" "$ROOT/tests/test_s1_synth_page_restore.cpp" "$BUILD/test_s1_synth_page_restore"
"$BUILD/test_s1_synth_page_restore"
