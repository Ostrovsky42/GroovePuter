#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_wav_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_kit_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_manifest_wire_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_logical_identity_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_transaction_pin_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_missing_relink_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_locator_sidecar_contract.py"
python3 "${ROOT_DIR}/tests/test_sampler_0_9_5_memory_policy_contract.py"

echo "0.9.5 sampler research contracts passed"
