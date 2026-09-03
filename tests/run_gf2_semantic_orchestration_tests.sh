#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

python3 tests/test_gf2_frozen_git_boundary.py
python3 tests/test_gf2_semantic_orchestration.py
python3 tools/orchestrate_semantic_analysis.py \
  --source-ref "${SOURCE_REF:-HEAD}" \
  --output-dir build/gf2-semantic-analysis-test \
  --fail-on-change

test -s build/gf2-semantic-analysis-test/semantic_census.json
test -s build/gf2-semantic-analysis-test/genre_diff.json
test -s build/gf2-semantic-analysis-test/reachability_report.json
test -s build/gf2-semantic-analysis-test/reachability_diff.json
test -s build/gf2-semantic-analysis-test/recipe_matrix.json
test -s build/gf2-semantic-analysis-test/pattern_statistics.json
test -s build/gf2-semantic-analysis-test/report.md

printf 'GF2 semantic analysis orchestration: OK\n'
