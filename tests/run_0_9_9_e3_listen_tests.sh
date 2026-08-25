#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e3-listen"
FIXTURE="${BUILD_DIR}/e3_listen_fixture_generated.h"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e3_listen_surface_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_0_9_9_e3r_b_source_guard.py"

python3 "${ROOT_DIR}/tests/generate_0_9_9_e3_listen_fixture.py" \
  --output "${FIXTURE}"
test -s "${FIXTURE}"

grep -q "inline constexpr uint8_t kCaseCount = 32;" "${FIXTURE}"
grep -q "inline constexpr uint8_t kBassRhythmCaseCount = 15;" "${FIXTURE}"
grep -q "kCanonical" "${FIXTURE}"
grep -q "kBefore" "${FIXTURE}"
grep -q "kAfter" "${FIXTURE}"

# Frozen V0R authority remains unchanged; E3L is only a consumer of frozen
# E3R-B evidence and cannot update graph/golden data.
bash "${ROOT_DIR}/tests/run_0_9_9_e3r_b_frozen_baseline.sh"

echo "0.9.9-E3L DROP / DISPLACE musical listen contracts: OK"
