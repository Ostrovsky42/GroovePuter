#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/e2t-activity-cadence"
mkdir -p "$BUILD"

CXX="${CXX:-g++}"

printf '%s\n' '=== E2t source ownership / scope regressions ==='
python3 "$ROOT/tests/test_0_9_9_e2t_source_regressions.py"

printf '%s\n' '=== E2t pure activity/cadence runtime contract ==='
"$CXX" \
  -std=c++17 -O2 -Wall -Wextra -Werror \
  -I"$ROOT" \
  "$ROOT/src/generation/generation_context.cpp" \
  "$ROOT/tests/test_0_9_9_e2t_activity_cadence.cpp" \
  -o "$BUILD/test_0_9_9_e2t_activity_cadence"

"$BUILD/test_0_9_9_e2t_activity_cadence"

printf '%s\n' '0.9.9-E2t activity/cadence focused tests: PASS'
