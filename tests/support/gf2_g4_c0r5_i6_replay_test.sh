#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
I6_BASE="c7be954f9739cf6f11b723b2dc094d303dc4f15b"
BUILD_DIR="${ROOT}/build/host-tests/gf2-g4-c0r5-i6-replay"
mkdir -p "${BUILD_DIR}"

echo "G4-C0R5 I6 replay base: ${I6_BASE}"
echo "G4-C0R5 I6 replay head: $(git -C "${ROOT}" rev-parse HEAD)"

# Replay instrumentation must not add any production delta on top of I6.
if ! git -C "${ROOT}" diff --quiet "${I6_BASE}...HEAD" -- src/; then
  echo "G4_C0R5_I6_REPLAY_FAIL reason=research replay changed src/" >&2
  git -C "${ROOT}" diff --stat "${I6_BASE}...HEAD" -- src/ >&2
  exit 1
fi
echo "G4-C0R5 I6 replay source firewall: src/ DELTA = NONE"

# RED until the exact C0R3/C0R4 production-backed observer and C0R5 analyzer
# are carried onto the I6-rooted replay branch. Do not reconstruct policy here.
required=(
  tests/support/build_gf2_gate_b_g4_c0r3_probe.sh
  tests/support/gf2_gate_b_g4_c0r5_full_role_census_test.sh
  tools/gf2/g4_c0r3_dump.cpp
  tools/gf2/g4_c0r3_tonal_probe.cpp
  tools/gf2/g4_c0r3_tonal_probe.h
  tools/gf2/g4_c0r4_bass_candidates_probe.cpp
  tools/gf2/g4_c0r4_bass_candidates_probe.h
  tools/gf2/g4_c0r4_role_plan_probe.cpp
  tools/gf2/g4_c0r4_role_plan_probe.h
  tools/gf2/g4_c0r5_role_coherence.py
)
for path in "${required[@]}"; do
  if [[ ! -f "${ROOT}/${path}" ]]; then
    echo "G4_C0R5_I6_REPLAY_RED missing_observer=${path}" >&2
    exit 1
  fi
done

# Build the same interception seam against I6 production first.
bash "${ROOT}/tests/support/build_gf2_gate_b_g4_c0r3_probe.sh"
BIN="${ROOT}/build/host-tests/gf2-gate-b/gf2_gate_b_dump_c0r3_gcc"

# Runtime profile ordinals are not assumed from documentation. Prove the four
# bindings through the production-backed replay binary before the 512-row run.
python3 - "${BIN}" <<'PY'
import csv
import subprocess
import sys

binary = sys.argv[1]
expected = {
    "0": "Acid/BASE",
    "9": "Dub/Reggae/Dub Techno",
    "20": "House/BASE",
    "27": "Drum&Bass/BASE",
}
for ordinal, profile_id in expected.items():
    completed = subprocess.run(
        [binary, "--g4-c0r3-dump", ordinal, "0", "0", "23", "P1"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    rows = list(csv.DictReader(completed.stdout.splitlines(), delimiter="\t"))
    assert len(rows) == 1, (ordinal, len(rows))
    row = rows[0]
    assert row["profile_ordinal"] == ordinal, row
    assert row["profile_id"] == profile_id, (ordinal, profile_id, row["profile_id"])
    assert row["migration_status"] == "APPLIED", row
    print("REPLAY_PROFILE", ordinal, profile_id)
PY

# Reuse the exact baseline C0R5 measurement contract and analyzer. It emits
# objective native-set and planning-vs-audible topology classes only.
bash "${ROOT}/tests/support/gf2_gate_b_g4_c0r5_full_role_census_test.sh"

SRC="${ROOT}/build/host-tests/gf2-gate-b/g4-c0r2-full/g4-c0r5-full-role-census"
cp "${SRC}/G4_C0R5_ROLE_COHERENCE_RAW.tsv" "${BUILD_DIR}/I6_C0R5_ROLE_COHERENCE_RAW.tsv"
cp "${SRC}/G4_C0R5_ROLE_COHERENCE_ROWS.tsv" "${BUILD_DIR}/I6_C0R5_ROLE_COHERENCE_ROWS.tsv"
cp "${SRC}/G4_C0R5_ROLE_COHERENCE_SUMMARY.tsv" "${BUILD_DIR}/I6_C0R5_ROLE_COHERENCE_SUMMARY.tsv"

echo "G4-C0R5 I6 replay: PASS"
