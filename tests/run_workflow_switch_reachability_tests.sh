#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
python3 tests/test_workflow_switch_reachability.py
printf '%s\n' 'Workflow switch reachability gate: PASS'
