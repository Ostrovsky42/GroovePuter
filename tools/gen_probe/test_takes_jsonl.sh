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
TUPLE=(--mode house --recipes 0 --takes "$TAKES" --phrase-bars 4 --p-level p3 --pattern-address 0xFFFF)

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
    --p-level p3 --pattern-address 0xFFFF --sequence undo >/dev/null 2>&1
  "$BIN" --emit-takes-jsonl "$WORK/s_$mode.jsonl" --mode "$mode" --recipes 0 --takes 2 --phrase-bars 4 \
    --p-level p3 --pattern-address 0xFFFF --sequence sequential >/dev/null 2>&1
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

# 5. clear (empty Song rows and their slots between presses, no Undo) == undo for k = 0..7.
for mode in house techno ukgarage acid; do
  "$BIN" --emit-takes-jsonl "$WORK/u8_$mode.jsonl" --mode "$mode" --recipes 0 --takes 8 --phrase-bars 4 \
    --p-level p3 --pattern-address 0xFFFF --sequence undo >/dev/null 2>&1
  "$BIN" --emit-takes-jsonl "$WORK/c8_$mode.jsonl" --mode "$mode" --recipes 0 --takes 8 --phrase-bars 4 \
    --p-level p3 --pattern-address 0xFFFF --sequence clear >/dev/null 2>&1
  python3 - "$WORK/u8_$mode.jsonl" "$WORK/c8_$mode.jsonl" "$mode" <<'PY'
import json, sys
def ev(p):
    return {(r["take"], r["bar_idx"]): json.dumps(r["events"], sort_keys=True) for r in map(json.loads, open(p, encoding="utf-8"))}
u, c, mode = ev(sys.argv[1]), ev(sys.argv[2]), sys.argv[3]
if len(u) != 32 or u != c:
    print(f"FAIL 5: {mode}: clear and undo differ for k=0..7 ({len(u)} vs {len(c)} bars)")
    sys.exit(1)
print(f"PASS 5: {mode} BASE P3: clear == undo for k=0..7 (32 bars)")
PY
done

# 6-9. GENRE G (--g-path genre), P3, House BASE.
"$BIN" --emit-takes-jsonl "$WORK/g_a.jsonl" --mode house --recipes 0 --takes "$TAKES" --p-level p3 --g-path genre >"$WORK/g_a.stdout" 2>/dev/null
"$BIN" --emit-takes-jsonl "$WORK/g_b.jsonl" --mode house --recipes 0 --takes "$TAKES" --p-level p3 --g-path genre >/dev/null 2>&1
cmp "$WORK/g_a.jsonl" "$WORK/g_b.jsonl"
echo "PASS 6: GENRE G, two runs of one tuple are byte-identical"
for k in $(seq 0 $((TAKES - 1))); do
  got=$(grep -E "^#ATT recipe=0 take=$k .*g_path=genre" "$WORK/g_a.stdout" | sed -E 's/.*next_ordinal=([0-9]+).*/\1/')
  if [ "$got" != "$((k + 1))" ]; then
    echo "FAIL 7: GENRE take $k has next_ordinal=$got, want $((k + 1))" >&2
    exit 1
  fi
done
echo "PASS 7: GENRE take k is the k-th press from a reset attempt table (k=0..$((TAKES - 1)))"
"$BIN" --emit-takes-jsonl "$WORK/g_s.jsonl" --mode house --recipes 0 --takes "$TAKES" --p-level p3 --g-path genre --sequence sequential >/dev/null 2>&1
python3 - "$WORK/g_a.jsonl" "$WORK/g_s.jsonl" <<'PY'
import json, sys
def ev(p): return [json.dumps(r["events"], sort_keys=True) for r in map(json.loads, open(p, encoding="utf-8"))]
a, s = ev(sys.argv[1]), ev(sys.argv[2])
if a != s:
    print(f"FAIL 8: GENRE undo-style and sequential differ on {sum(x!=y for x,y in zip(a,s))} takes"); sys.exit(1)
print(f"PASS 8: GENRE undo-style == sequential (one press after another in one project), {len(a)} takes")
PY
# MATERIAL vs GENRE: only k=0 is required to match (it does); the rest is reported by the lab, not asserted.
"$BIN" --emit-takes-jsonl "$WORK/m_a.jsonl" --mode house --recipes 0 --takes 1 --p-level p3 --pattern-address 0xFFFF >/dev/null 2>&1
python3 - "$WORK/g_a.jsonl" "$WORK/m_a.jsonl" <<'PY'
import json, sys
g = [json.loads(l) for l in open(sys.argv[1], encoding="utf-8")]
m = [json.loads(l) for l in open(sys.argv[2], encoding="utf-8")]
if g[0]["events"] != m[0]["events"]:
    print("FAIL 9: GENRE take 0 bar 1 differs from MATERIAL take 0 bar 1"); sys.exit(1)
print("PASS 9: GENRE take 0 == MATERIAL take 0, bar 1 (events identical; later takes are not identical)")
PY
# Leak check for GENRE: House k=0 after a warm-up of Techno, UKG, Acid in the same process.
"$BIN" --emit-takes-jsonl "$WORK/gl_fresh.jsonl" --mode house --recipes 0 --takes 1 --p-level p3 --g-path genre >/dev/null 2>&1
"$BIN" --emit-takes-jsonl "$WORK/gl_warm.jsonl" --mode house --recipes 0 --takes 1 --p-level p3 --g-path genre --warmup techno,ukgarage,acid >/dev/null 2>&1
cmp "$WORK/gl_fresh.jsonl" "$WORK/gl_warm.jsonl"
echo "PASS 10: GENRE House k=0 is the same after Techno, UKG, Acid in the same process"
