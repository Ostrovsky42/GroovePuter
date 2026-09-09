#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE="fcd0d77da5ed6ef38419547477ab26e77ec6ff26"
LEGACY_REF="refs/remotes/origin/research/20260903-05-0.9.10-gf2-c2-gate-b-materialized-capacity"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
GENERATED="${BUILD_DIR}/generated-c0r-preflight"
REPLAY="${BUILD_DIR}/generated-c0r-preflight-replay"

cd "${ROOT}"

SOURCE_FIREWALL_HEAD="HEAD"
if [[ -n "${GITHUB_HEAD_REF:-}" ]]; then
  SOURCE_FIREWALL_HEAD="refs/remotes/origin/${GITHUB_HEAD_REF}"
fi

git cat-file -e "${BASE}^{commit}"
git show-ref --verify --quiet "${LEGACY_REF}"
git show-ref --verify --quiet "${SOURCE_FIREWALL_HEAD}"

echo "G4-C0R Gate B integration base: ${BASE}"
echo "G4-C0R Gate B source firewall head: ${SOURCE_FIREWALL_HEAD}"
git diff --check "${BASE}...${SOURCE_FIREWALL_HEAD}"
if [[ -n "$(git diff --name-only "${BASE}...${SOURCE_FIREWALL_HEAD}" -- src/)" ]]; then
  echo "G4-C0R Gate B source firewall FAILED: research src/ delta is non-empty" >&2
  git diff --name-status "${BASE}...${SOURCE_FIREWALL_HEAD}" -- src/ >&2
  exit 1
fi
echo "G4-C0R Gate B source firewall: src/ DELTA = NONE"

for name in \
  GF2_GATE_B_MATERIALIZED_CORPUS.tsv \
  GF2_GATE_B_PROFILE_SIGNATURES.tsv \
  GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv \
  GF2_GATE_B_FINDINGS.md; do
  path="docs/research/${name}"
  if ! git diff --quiet "${LEGACY_REF}" "${SOURCE_FIREWALL_HEAD}" -- "${path}"; then
    echo "G4-C0R legacy Gate B artifact changed: ${path}" >&2
    exit 1
  fi
done
echo "G4-C0R legacy Gate B committed artifacts: BYTE-UNCHANGED"

python3 tests/test_gf2_gate_b_analysis.py
python3 tests/test_gf2_gate_b_finalize.py
bash tests/run_gf2_gate_b_dump_tests.sh

rm -rf "${GENERATED}" "${REPLAY}"
python3 tools/gf2_gate_b.py \
  --raw "${BUILD_DIR}/raw.tsv" \
  --seeds tests/support/gf2_gate_b_seeds.tsv \
  --contract tests/support/gf2_gate_b_contract.json \
  --output-dir "${GENERATED}"
python3 tools/gf2_gate_b_finalize.py \
  --raw "${BUILD_DIR}/raw.tsv" \
  --generated-dir "${GENERATED}"
python3 tools/gf2_gate_b.py \
  --raw "${BUILD_DIR}/raw.tsv" \
  --seeds tests/support/gf2_gate_b_seeds.tsv \
  --contract tests/support/gf2_gate_b_contract.json \
  --output-dir "${REPLAY}"
python3 tools/gf2_gate_b_finalize.py \
  --raw "${BUILD_DIR}/raw.tsv" \
  --generated-dir "${REPLAY}"

for name in \
  GF2_GATE_B_MATERIALIZED_CORPUS.tsv \
  GF2_GATE_B_PROFILE_SIGNATURES.tsv \
  GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv \
  GF2_GATE_B_FINDINGS.md; do
  cmp "${GENERATED}/${name}" "${REPLAY}/${name}"
done

echo "G4-C0R Gate B analyzer/finalizer replay: BYTE-IDENTICAL"
echo "G4-C0R Gate B integration-base preflight: OK"
