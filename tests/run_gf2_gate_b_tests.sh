#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE="9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
GENERATED="${BUILD_DIR}/generated"
REPLAY="${BUILD_DIR}/generated-replay"
COMMITTED="${ROOT}/docs/research"
ALLOW_UNFROZEN="${GF2_GATE_B_ALLOW_UNFROZEN:-0}"

cd "${ROOT}"

echo "GF2-C2 Gate B exact base: ${BASE}"
git cat-file -e "${BASE}^{commit}"
git diff --check "${BASE}...HEAD"
if [[ -n "$(git diff --name-only "${BASE}...HEAD" -- src/)" ]]; then
  echo "GF2-C2 Gate B source firewall FAILED: src/ delta is non-empty" >&2
  git diff --name-status "${BASE}...HEAD" -- src/ >&2
  exit 1
fi
echo "GF2-C2 Gate B source firewall: src/ DELTA = NONE"

python3 tests/test_gf2_gate_b_analysis.py
bash tests/run_gf2_gate_b_dump_tests.sh

rm -rf "${GENERATED}" "${REPLAY}"
python3 tools/gf2_gate_b.py \
  --raw "${BUILD_DIR}/raw.tsv" \
  --seeds tests/support/gf2_gate_b_seeds.tsv \
  --contract tests/support/gf2_gate_b_contract.json \
  --output-dir "${GENERATED}"
python3 tools/gf2_gate_b.py \
  --raw "${BUILD_DIR}/raw.tsv" \
  --seeds tests/support/gf2_gate_b_seeds.tsv \
  --contract tests/support/gf2_gate_b_contract.json \
  --output-dir "${REPLAY}"

for name in \
  GF2_GATE_B_MATERIALIZED_CORPUS.tsv \
  GF2_GATE_B_PROFILE_SIGNATURES.tsv \
  GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv \
  GF2_GATE_B_FINDINGS.md; do
  cmp "${GENERATED}/${name}" "${REPLAY}/${name}"
done
echo "Gate B analyzer replay: BYTE-IDENTICAL"

python3 - "${BUILD_DIR}/raw.tsv" "${GENERATED}" tests/support/gf2_gate_b_contract.json <<'PY'
import csv
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

raw_path = Path(sys.argv[1])
out = Path(sys.argv[2])
contract = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8"))

with raw_path.open(newline="", encoding="utf-8") as handle:
    raw = list(csv.DictReader(handle, delimiter="\t"))
with (out / "GF2_GATE_B_MATERIALIZED_CORPUS.tsv").open(newline="", encoding="utf-8") as handle:
    corpus = list(csv.DictReader(handle, delimiter="\t"))
with (out / "GF2_GATE_B_PROFILE_SIGNATURES.tsv").open(newline="", encoding="utf-8") as handle:
    profiles = list(csv.DictReader(handle, delimiter="\t"))
with (out / "GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv").open(newline="", encoding="utf-8") as handle:
    pairs = list(csv.DictReader(handle, delimiter="\t"))

profile_count = contract["profile_count"]
seed_count = contract["seed_count"]
depths = tuple(contract["depths"])
expected_realizations = profile_count * seed_count * len(depths)
expected_pairs = profile_count * (profile_count - 1) // 2

assert len(raw) == expected_realizations == 5445, (len(raw), expected_realizations)
assert len(corpus) == expected_realizations, len(corpus)
assert len(profiles) == profile_count == 33, len(profiles)
assert len(pairs) == expected_pairs == contract["pairwise_count"] == 528, len(pairs)

profile_ids = {row["profile_id"] for row in raw}
assert len(profile_ids) == profile_count
seeds = {row["seed"] for row in raw}
assert len(seeds) == seed_count
coverage = defaultdict(set)
for row in raw:
    coverage[(row["profile_id"], row["seed"])].add(row["depth"])
assert coverage
assert all(value == set(depths) for value in coverage.values())

pair_keys = {(row["profile_a"], row["profile_b"]) for row in pairs}
assert len(pair_keys) == expected_pairs
assert all(a < b for a, b in pair_keys), "pairwise ordering is not canonical"
assert not any((b, a) in pair_keys for a, b in pair_keys), "A-B/B-A duplicate found"

classes = {
    "STRUCTURALLY DISTINCT",
    "PARTIALLY DISTINCT",
    "TIMBRE-DEPENDENT",
    "STRUCTURALLY REDUNDANT",
    "INSUFFICIENT EVIDENCE",
}
assert {row["classification"] for row in pairs} <= classes
assert all(
    row["rhythm_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    and row["bass_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    and row["harmony_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    and row["phrase_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    and row["role_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    and row["transformation_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    and row["negative_relation"] in {"SAME", "DISJOINT", "OVERLAP", "NOT_OBSERVED"}
    for row in pairs
)

laws = {row["declared_phrase_law"] for row in raw if row["declared_phrase_law"] != "NOT_OBSERVED"}
assert {"LOOP", "REPEAT/REPLY", "DEVELOP/RETURN", "SPARSE DRIFT"} <= laws, laws
assert any(row["phrase_admitted"] == "YES" for row in raw)

applied = [row for row in raw if row["migration_status"] == "APPLIED"]
assert applied, "no applied production materializations"
assert all(row["v0r_requested_result_effective"] == "YES" for row in applied)
roles = {row["synth_b_role"] for row in applied if row["synth_b_role"] != "NOT_OBSERVED"}
assert roles, "secondary role observation is empty"
assert any(row["chord_applied"] == "YES" for row in applied), "no Chord participation observed"
assert any(row["melodic_applied"] == "YES" for row in applied), "no Melodic participation observed"
assert any(
    row["chord_applied"] == "YES" and row["melodic_applied"] == "YES"
    for row in applied
), "no Hybrid participation observed"

assert all(row["physical_duration"] == "NOT_OBSERVED" for row in raw)
assert all(row["physical_duration"] == "NOT_OBSERVED" for row in corpus)

classification_counts = Counter(row["classification"] for row in pairs)
print(f"Gate B profiles: {len(profile_ids)}")
print(f"Gate B seeds: {len(seeds)}")
print(f"Gate B realizations: {len(corpus)}")
print(f"Gate B unordered pairs: {len(pairs)}")
for classification in sorted(classes):
    print(f"Gate B pair {classification}: {classification_counts[classification]}")
print(f"Gate B phrase laws: {','.join(sorted(laws))}")
print(f"Gate B observed Synth-B role ordinals (provenance only): {','.join(sorted(roles))}")
PY

FINDINGS="${GENERATED}/GF2_GATE_B_FINDINGS.md"
for heading in \
  "## 1. Exact base" \
  "## 2. Corpus" \
  "## 3. Determinism" \
  "## 4. Measured axis capacity" \
  "## 5. Pairwise results" \
  "## 6. Same-Genre Recipe collisions" \
  "## 7. Cross-Genre collisions" \
  "## 8. One-dimensional profiles" \
  "## 9. Negative capacity" \
  "## 10. Observation limitations" \
  "## 11. Gate B conclusion"; do
  grep -Fq "${heading}" "${FINDINGS}"
done

grep -Fq 'GRID 8/32' "${FINDINGS}"
grep -Fq 'ROLE HIERARCHY VIA DEPTH' "${FINDINGS}"
grep -Fq 'TIMBRE-DEPENDENT' "${FINDINGS}"
grep -Fq 'NOT_OBSERVED' "${FINDINGS}"

if [[ "${ALLOW_UNFROZEN}" == "1" ]]; then
  echo "Gate B bootstrap mode: generated artifacts validated; committed snapshot equality intentionally deferred"
else
  for name in \
    GF2_GATE_B_MATERIALIZED_CORPUS.tsv \
    GF2_GATE_B_PROFILE_SIGNATURES.tsv \
    GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv \
    GF2_GATE_B_FINDINGS.md; do
    test -f "${COMMITTED}/${name}"
    cmp "${GENERATED}/${name}" "${COMMITTED}/${name}"
  done
  echo "Gate B committed artifacts: BYTE-IDENTICAL"
fi

echo "GF2-C2 Gate B focused proof: OK"
