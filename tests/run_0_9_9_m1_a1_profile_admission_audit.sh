#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)";B="$R/build/host-tests/m1-a1";mkdir -p "$B"
python3 "$R/tests/test_0_9_9_m1_a1_profile_admission_audit.py" >"$B/gcc1"
python3 "$R/tests/test_0_9_9_m1_a1_profile_admission_audit.py" >"$B/gcc2"
diff -u "$B/gcc1" "$B/gcc2"
printf 'M1-A1 source audit: PASS\n'
