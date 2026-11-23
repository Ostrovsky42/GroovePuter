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
python3 -m gplab validate --takes "$WORK/a.jsonl" --profiles out/demo/profiles.json \
  --config config/pilot_house.yaml --out "$WORK/validate" >"$WORK/validate.log" 2>&1 || true
if grep -q "Traceback" "$WORK/validate.log"; then
  echo "FAIL 3: gplab could not parse the takes file:" >&2
  tail -20 "$WORK/validate.log" >&2
  exit 1
fi
echo "PASS 3: gplab parsed the takes file (validation verdict not interpreted: synthetic profiles)"
