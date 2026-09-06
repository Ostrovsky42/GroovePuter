#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 tests/test_ui_constitution_u1d_residency_continuity.py

echo "UI Constitution U1D focused gate: PASS"
