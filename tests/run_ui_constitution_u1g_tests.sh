#!/usr/bin/env bash
set -euo pipefail
cd "$(cd "$(dirname "$0")/.." && pwd)"
python3 tests/test_ui_constitution_u1g_tab_grammar.py
echo "U1G Tab grammar PASS"
