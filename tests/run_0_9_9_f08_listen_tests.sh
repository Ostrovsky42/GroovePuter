#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT="${ROOT}/build/host-tests/f08_listen_fixture_generated.h"
mkdir -p "$(dirname "${OUTPUT}")"

python3 "${ROOT}/tests/test_0_9_9_f08_listen_surface_source_regressions.py"
python3 "${ROOT}/tests/generate_0_9_9_f08_listen_fixture.py" \
  --output "${OUTPUT}"
test -s "${OUTPUT}"

echo "0.9.9-F08 LISTEN host contract: OK"
