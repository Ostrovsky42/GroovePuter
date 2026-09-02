#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

bash "${ROOT_DIR}/tests/run_midi_output_route_projection_0_9_7_tests.sh"
python3 "${ROOT_DIR}/tests/test_midi_settings_boot_order_0_9_7_source_regressions.py"

echo "0.9.7-R5a MIDI settings boot order: PASS"
