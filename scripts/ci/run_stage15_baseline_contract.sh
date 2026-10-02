#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/host-tests

python3 tests/test_stage15_tonal_fixture_roles.py

bash tests/run_stage15_tonal_baseline_dump.sh \
  > build/host-tests/stage15_tonal_legacy_actual.tsv
test "$(wc -l < build/host-tests/stage15_tonal_legacy_actual.tsv)" -eq 257
diff -u \
  tests/data/stage15_tonal_legacy_baseline.tsv \
  build/host-tests/stage15_tonal_legacy_actual.tsv

base64 -d tests/data/stage15_tonal_enabled_f13_baseline.tsv.gz.b64 \
  | gzip -dc > build/host-tests/stage15_tonal_enabled_f13_expected.tsv
test "$(wc -l < build/host-tests/stage15_tonal_enabled_f13_expected.tsv)" -eq 257

# HISTORICAL PRE-F13 -> F13
python3 tests/test_stage15_tonal_f13_corpus.py \
  tests/data/stage15_tonal_enabled_pre_f13_baseline.tsv \
  build/host-tests/stage15_tonal_enabled_f13_expected.tsv

# CURRENT ACCEPTED GOLDEN
bash tests/run_stage15_tonal_baseline_dump.sh --tonal \
  > build/host-tests/stage15_tonal_enabled_actual.tsv
test "$(wc -l < build/host-tests/stage15_tonal_enabled_actual.tsv)" -eq 257
diff -u \
  tests/data/stage15_tonal_enabled_baseline.tsv \
  build/host-tests/stage15_tonal_enabled_actual.tsv

# HISTORICAL OWNERSHIP BOUNDARY
python3 tests/test_stage15_tonal_corpus_boundary.py
