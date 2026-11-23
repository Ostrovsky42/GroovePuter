#!/usr/bin/env bash
# Host test for gen_probe --emit-takes-jsonl (genre lab PR-B). Host only, needs SDL2 dev packages
# to build genrefeatures. Checks:
#   1. same tuple twice -> byte-identical file
#   2. take k is the k-th press of G from a reset attempt table (next_ordinal == k+1)
#   3. gplab parses the file (python -m gplab validate; PASS/FAIL is not the check here)
# It does NOT compare against the device. That check (c) is manual and not done here.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${1:-$(mktemp -d)}"
mkdir -p "$WORK"
BIN="$WORK/genrefeatures"
bash "$ROOT/tools/gen_probe/build.sh" "$ROOT/tools/gen_probe/genrefeatures.cpp" "$BIN"

TAKES=8
TUPLE=(--mode house --recipes 0 --takes "$TAKES" --phrase-bars 4 --p-level p2 --pattern-address 0xFFFF)

"$BIN" --emit-takes-jsonl "$WORK/a.jsonl" "${TUPLE[@]}" >"$WORK/a.stdout" 2>"$WORK/a.stderr"
"$BIN" --emit-takes-jsonl "$WORK/b.jsonl" "${TUPLE[@]}" >"$WORK/b.stdout" 2>"$WORK/b.stderr"

cmp "$WORK/a.jsonl" "$WORK/b.jsonl"
echo "PASS 1: two runs of one tuple are byte-identical ($(wc -l <"$WORK/a.jsonl") lines)"

for k in $(seq 0 $((TAKES - 1))); do
  want=$((k + 1))
  got=$(grep -E "^#ATT recipe=0 take=$k " "$WORK/a.stdout" | sed -E 's/.*next_ordinal=//')
  if [ "$got" != "$want" ]; then
    echo "FAIL 2: take $k has next_ordinal=$got, want $want" >&2
    exit 1
  fi
done
echo "PASS 2: take k is the k-th press from a reset attempt table (next_ordinal = k+1, k=0..$((TAKES - 1)))"

cd "$ROOT/scripts/genre-lab"
if [ ! -f out/demo/profiles.json ]; then
  echo "FAIL 3: run 'make demo' in scripts/genre-lab first (needs out/demo/profiles.json)" >&2
  exit 1
fi
PYTHONPATH=src python3 -m gplab validate --takes "$WORK/a.jsonl" --profiles out/demo/profiles.json \
  --config config/pilot_house.yaml --out "$WORK/validate" >"$WORK/validate.log" 2>&1 || true
# The report header is the proof that gplab ran and parsed every row. A missing
# module or a traceback leaves no such header.
if ! grep -q "^# Validation:" "$WORK/validate.log"; then
  echo "FAIL 3: gplab did not produce a validation report:" >&2
  tail -20 "$WORK/validate.log" >&2
  exit 1
fi
echo "PASS 3: gplab parsed the takes file (validation verdict not interpreted: synthetic profiles)"

# 4. sequential == undo for the takes sequential can produce (k = 0, 1): the Song
#    holds two 4-bar takes in one project, so k >= 2 is not comparable (see PR body).
for mode in house techno ukgarage acid; do
  "$BIN" --emit-takes-jsonl "$WORK/u_$mode.jsonl" --mode "$mode" --recipes 0 --takes 2 --phrase-bars 4 \
    --p-level p2 --pattern-address 0xFFFF --sequence undo >/dev/null 2>&1
  "$BIN" --emit-takes-jsonl "$WORK/s_$mode.jsonl" --mode "$mode" --recipes 0 --takes 2 --phrase-bars 4 \
    --p-level p2 --pattern-address 0xFFFF --sequence sequential >/dev/null 2>&1
  python3 - "$WORK/u_$mode.jsonl" "$WORK/s_$mode.jsonl" "$mode" <<'PY'
import json, sys
def ev(p):
    return {(r["take"], r["bar_idx"]): json.dumps(r["events"], sort_keys=True)
            for r in map(json.loads, open(p, encoding="utf-8"))}
u, s, mode = ev(sys.argv[1]), ev(sys.argv[2]), sys.argv[3]
if not u or u != s:
    print(f"FAIL 4: {mode}: undo and sequential events differ ({len(u)} vs {len(s)} bars)")
    sys.exit(1)
print(f"PASS 4: {mode} BASE: sequential == undo, {len(u)} bars (take 0-1)")
PY
done
