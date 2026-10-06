#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/h0-r1}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"

cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then
  echo "H0-R1 audition ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -Wno-unused-parameter -Wno-unused-function \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tools/h0_r1/h0_r1_house_audition.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$OUT/h0_r1_house_audition"

ROOT_OUT="$(cd "$OUT" && pwd)"
GP_TOOL_WORK="$(mktemp -d "${TMPDIR:-/tmp}/gp-h0-r1-audition.XXXXXX")"
trap 'rm -rf "$GP_TOOL_WORK"' EXIT
(cd "$GP_TOOL_WORK" && H0_R1_OUT="$ROOT_OUT" \
  "$ROOT_OUT/h0_r1_house_audition")

python3 - "$ROOT_OUT/report.tsv" <<'PY'
import csv
import sys

expected_header = [
    "variant", "genre", "recipe", "policy", "bar", "source_ordinal",
    "harmonic_root_pc", "synth_a_pitch_classes", "synth_b_pitch_classes",
]
with open(sys.argv[1], newline="", encoding="utf-8") as source:
    rows = list(csv.DictReader(source, delimiter="\t"))
if not rows or list(rows[0]) != expected_header:
    raise SystemExit("H0-R1 report schema mismatch")
if any(None in row or len(row) != len(expected_header) for row in rows):
    raise SystemExit("H0-R1 report contains malformed rows")

def variant(name):
    return [row for row in rows if row["variant"] == name]

half = variant("house_halfbar")
slow = variant("house_slow")
fixture = variant("slow_c_major_popcycle_fixture")
if len(half) != 8 or len(slow) != 4 or len(fixture) != 4:
    raise SystemExit("H0-R1 report has unexpected harmonic event counts")
for name in ("house_slow", "slow_c_major_popcycle_fixture"):
    found = [(int(r["bar"]), int(r["harmonic_root_pc"])) for r in variant(name)]
    if found != [(0, 0), (1, 7), (2, 9), (3, 5)]:
        raise SystemExit(f"{name} roots are not C/G/A/F: {found}")
if any(int(r["bar"]) != int(r["source_ordinal"]) // 2 for r in half):
    raise SystemExit("HALF-BAR report did not preserve both in-bar states")
for row in fixture:
    root = str(row["harmonic_root_pc"])
    if root not in row["synth_b_pitch_classes"].split(","):
        raise SystemExit("controlled fixture report lacks produced Synth B root")
print("H0-R1 report schema and materialized roots: PASS")
PY

echo "H0-R1 House listening package: $ROOT_OUT"
