#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

bash "${ROOT_DIR}/tests/run_sampler_registry_boot_tests.sh"
python3 "${ROOT_DIR}/tests/test_sampler_folder_browser_source_regressions.py"

echo "sampler folder browser 0.9.5-A tests passed"
