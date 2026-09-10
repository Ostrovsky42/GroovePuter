#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAILED=0

echo "== test_ui_constitution_u1c_frame_snapshot.py =="
if ! python3 "${ROOT_DIR}/tests/test_ui_constitution_u1c_frame_snapshot.py"; then
  FAILED=1
fi

if [[ "${FAILED}" -ne 0 ]]; then
  echo "UI Constitution U1C focused gate: FAIL"
  exit 1
fi

echo "UI Constitution U1C focused gate: PASS"
