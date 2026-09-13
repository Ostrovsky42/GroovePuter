#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${ROOT_DIR}/tests/test_0_9_11_c5_material_ux_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_0_9_11_c5_style_projection_source_regressions.py"
