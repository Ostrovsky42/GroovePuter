#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_wav_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_kit_contract.py"

echo "0.9.5 sampler research contracts passed"
