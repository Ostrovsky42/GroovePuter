#!/usr/bin/env bash
# PML-B proof (host). Runs in a private temp dir; never touches repository data paths.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/pml/b}"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
CXX="${CXX:-g++}"
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile)
"$CXX" -std=c++17 -w -Wno-c++20-extensions -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tools/pml/pml_b_proof.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$OUT/pml_b_proof"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/pml-b.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
{ echo "exact SHA: $(git -C "$ROOT" rev-parse HEAD)  (dirty files: $(git -C "$ROOT" status --porcelain | grep -vc '^??'))"
  (cd "$WORK" && "$OUT/pml_b_proof" 2>&1 | grep -v '^  - \|^    - \|^TempoDelay\|^\[[A-Za-z]' ); } | tee "$OUT/pml_b_report.txt"
