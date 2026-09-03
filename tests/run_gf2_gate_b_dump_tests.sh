#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
mkdir -p "${BUILD_DIR}"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)
SOURCES+=("/src/generation/migration/phrase_execution.cpp")

compile_dump() {
  local compiler="$1"
  local output="$2"
  "${compiler}" -std=c++17 -O2 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-variable -Wno-unused-but-set-variable \
    -I"${ROOT}" -I"${ROOT}/platform_sdl" \
    -include "${ROOT}/platform_sdl/arduino_compat.h" \
    "${SOURCES[@]/#/${ROOT}}" \
    "${ROOT}/src/dsp/genre_manager.cpp" \
    "${ROOT}/scenes.cpp" "${ROOT}/json_evented.cpp" \
    "${ROOT}/src/audio/pattern_paging.cpp" \
    "${ROOT}/tools/gf2/gf2_gate_b_dump.cpp" \
    -o "${output}"
}

GCC_BIN="${BUILD_DIR}/gf2_gate_b_dump_gcc"
compile_dump "${CXX:-g++}" "${GCC_BIN}"

"${GCC_BIN}" "${ROOT}/tests/support/gf2_gate_b_seeds.tsv" \
  > "${BUILD_DIR}/raw.tsv" 2> "${BUILD_DIR}/meta.txt"
"${GCC_BIN}" "${ROOT}/tests/support/gf2_gate_b_seeds.tsv" \
  > "${BUILD_DIR}/raw-repeat.tsv" 2> "${BUILD_DIR}/meta-repeat.txt"
cmp "${BUILD_DIR}/raw.tsv" "${BUILD_DIR}/raw-repeat.tsv"
cmp "${BUILD_DIR}/meta.txt" "${BUILD_DIR}/meta-repeat.txt"

echo "Gate B GCC replay: BYTE-IDENTICAL"

if command -v clang++ >/dev/null 2>&1; then
  CLANG_BIN="${BUILD_DIR}/gf2_gate_b_dump_clang"
  compile_dump clang++ "${CLANG_BIN}"
  "${CLANG_BIN}" "${ROOT}/tests/support/gf2_gate_b_seeds.tsv" \
    > "${BUILD_DIR}/raw-clang.tsv" 2> "${BUILD_DIR}/meta-clang.txt"
  cmp "${BUILD_DIR}/raw.tsv" "${BUILD_DIR}/raw-clang.tsv"
  cmp "${BUILD_DIR}/meta.txt" "${BUILD_DIR}/meta-clang.txt"
  echo "Gate B GCC/Clang dump: BYTE-IDENTICAL"
fi

python3 - "${BUILD_DIR}/raw.tsv" "${BUILD_DIR}/meta.txt" \
  "${ROOT}/tests/support/gf2_gate_b_contract.json" \
  "${ROOT}/tests/support/gf2_gate_b_seeds.tsv" <<'PY'
import csv
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

raw_path = Path(sys.argv[1])
meta_path = Path(sys.argv[2])
contract_path = Path(sys.argv[3])
seeds_path = Path(sys.argv[4])
contract = json.loads(contract_path.read_text(encoding="utf-8"))
seeds = [line.strip() for line in seeds_path.read_text(encoding="utf-8").splitlines()[1:] if line.strip()]
assert len(seeds) == contract["seed_count"] == 55
assert len(set(seeds)) == 55

meta = {}
for line in meta_path.read_text(encoding="utf-8").splitlines():
    row_type, key, value = line.split("\t")
    assert row_type == "META"
    meta[key] = int(value)
assert meta["profile_count"] == contract["profile_count"], (meta, contract)
assert meta["seed_count"] == contract["seed_count"]
assert meta["depth_count"] == len(contract["depths"])

with raw_path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))

expected = contract["profile_count"] * contract["seed_count"] * len(contract["depths"])
assert len(rows) == expected == 5445, (len(rows), expected)
keys = {(r["profile_id"], r["seed"], r["depth"]) for r in rows}
assert len(keys) == expected

coverage = defaultdict(set)
for row in rows:
    coverage[(row["profile_id"], row["seed"])].add(row["depth"])
assert coverage
assert all(value == set(contract["depths"]) for value in coverage.values())

header = set(rows[0]) if rows else set()
for forbidden in ("engine", "oscillator", "sample", "kit", "fx", "fx_param", "velocity"):
    assert forbidden not in header
assert {r["physical_duration"] for r in rows} == {"NOT_OBSERVED"}

laws = {r["declared_phrase_law"] for r in rows if r["declared_phrase_law"] != "NOT_OBSERVED"}
required_laws = {"LOOP", "REPEAT/REPLY", "DEVELOP/RETURN", "SPARSE DRIFT"}
assert required_laws <= laws, (required_laws, laws)

admission = Counter(r["phrase_admitted"] for r in rows)
assert admission["YES"] > 0

accepted = [r for r in rows if r["migration_status"] == "APPLIED"]
assert accepted, "production corpus produced no accepted material"
assert all(r["v0r_requested_result_effective"] == "YES" for r in accepted)

failed = [r for r in rows if r["migration_status"] != "APPLIED"]
assert failed, "Gate B missing-observation regression requires failed/non-applied production witnesses"
physical_fields = (
    "kick_onsets",
    "backbeat_onsets",
    "hat_onsets",
    "support_onsets",
    "kick_accents",
    "backbeat_accents",
    "hat_accents",
    "support_accents",
    "drum_timing",
    "synth_a_onsets",
    "synth_b_onsets",
    "synth_a_accents",
    "synth_b_accents",
    "synth_a_ghosts",
    "synth_b_ghosts",
    "synth_a_timing",
    "synth_b_timing",
    "synth_a_pitch_class",
    "synth_b_pitch_class",
    "synth_a_contour",
    "synth_b_contour",
    "harmonic_event_onsets",
    "harmonic_event_count",
    "chord_onsets",
    "melodic_fill_onsets",
    "chord_applied",
    "melodic_applied",
    "synth_b_role",
    "physical_event_count",
    "silence_mask",
)
for row in failed:
    for field in physical_fields:
        assert row[field] == "NOT_OBSERVED", (
            "failed/non-applied materialization serialized physical evidence",
            row["profile_id"],
            row["seed"],
            row["depth"],
            row["migration_status"],
            field,
            row[field],
        )

print(f"Gate B profiles: {meta['profile_count']}")
print(f"Gate B seeds: {meta['seed_count']}")
print(f"Gate B deterministic realizations: {len(rows)}")
print(f"Gate B failed/non-applied realizations: {len(failed)}")
print(f"Gate B phrase laws observed: {','.join(sorted(laws))}")
print(f"Gate B phrase admission: YES={admission['YES']} NO={admission['NO']}")
PY

echo "GF2-C2 Gate B production dump contract: OK"
