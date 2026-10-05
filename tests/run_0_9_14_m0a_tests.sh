#!/usr/bin/env bash
# M0-A gate: product firewall + reproducible structural corpus / listening set.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/0-9-14-m0a"
OUT="$ROOT/build/m0a"
mkdir -p "$BUILD" "$OUT"
CXX="${CXX:-g++}"

cd "$ROOT"
source "$ROOT/tests/lib/isolated_workdir.sh"
isolated_init

echo "== M0-A product firewall (runtime) =="
pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then echo "M0-A ERROR: failed to resolve SDL source set" >&2; exit 3; fi
"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 -DGROOVEPUTER_M1_TEST_PROBE \
  "${SRCS[@]}" ../tests/test_0_9_14_m0a_product_firewall.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$BUILD/test_0_9_14_m0a_product_firewall"
popd >/dev/null
isolated_run "$BUILD/test_0_9_14_m0a_product_firewall"

echo "== M0-A source regressions =="
python3 "$ROOT/tests/test_0_9_14_m0a_source_regressions.py"

echo "== M0-A structural corpus + listening set (tools/m0) =="
bash "$ROOT/tools/m0/build_m0a.sh" "$OUT" > "$OUT/m0a_stdout.txt"
REPORT="$OUT/m0a_report.txt"
for section in M0A-1 M0A-2 M0A-3 M0A-4 M0A-5 M0A-6 M0A-7 M0A-8 M0A-9 M0A-10 M0A-11; do
  grep -q "== $section " "$REPORT" || { echo "M0-A ERROR: report section $section missing" >&2; exit 4; }
done
grep -q "M0-A tool self-checks: PASS" "$REPORT" || { echo "M0-A ERROR: tool self-checks failed" >&2; exit 5; }
H1=$(grep -o "run1=[0-9a-f]*" "$REPORT" | cut -d= -f2)
H2=$(grep -o "run2=[0-9a-f]*" "$REPORT" | cut -d= -f2)
[[ -n "$H1" && "$H1" == "$H2" ]] || { echo "M0-A ERROR: corpus is not deterministic ($H1 vs $H2)" >&2; exit 6; }

# Listening set: every OK manifest row has a valid SMF; every GAP is explicit.
python3 - "$OUT/listening" <<'PY'
import sys, csv, pathlib
d = pathlib.Path(sys.argv[1])
rows = list(csv.DictReader((d / "manifest.tsv").open(), delimiter="\t"))
ok = [r for r in rows if r["status"].startswith("OK")]
gap = [r for r in rows if r["status"].startswith("GAP")]
assert ok and len(ok) + len(gap) == len(rows), "manifest incomplete"
for r in ok:
    data = (d / r["file"]).read_bytes()
    assert data[:4] == b"MThd" and b"MTrk" in data, f"bad SMF {r['file']}"
    assert (d / "listening_cards.md").read_text().count(r["file"]) >= 1, f"no card for {r['file']}"
for name in ("oneminute_Acid_loop.mid", "oneminute_Techno_takes.mid"):
    assert (d / name).read_bytes()[:4] == b"MThd", name
print(f"listening set: {len(ok)} files OK, {len(gap)} explicit GAP")
PY

echo "0.9.14 M0-A: GREEN"
