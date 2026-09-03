#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

python3 tests/test_gf2_c1f_final_static_semantic_census.py
python3 tests/test_gf2_c1rf_final_semantic_reachability.py
python3 tests/test_gf2_c1df_final_distinctness_dependency.py

echo "GF2-C1DF final distinctness dependency/loss map: OK"
