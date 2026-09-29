#!/usr/bin/env bash
# P0 preparation corpus: structural table, P2/P3 depth check, mix variants (host only).
# Usage: tools/m0/build_m0a_p0.sh [output-dir]     (default: build/m0a/p0)
# Produces WAV per scenario, manifest.tsv (cue tables) and, if ffmpeg exists, MP3s.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/m0a/p0}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions -Wno-unused-parameter -Wno-unused-function \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tools/m0/m0a_p0.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$OUT/m0a_p0"
cd "$ROOT"
if [[ -n "${M0_P0_BUILD_ONLY:-}" ]]; then exit 0; fi
M0_P0_OUT="$OUT" "$OUT/m0a_p0" | tee "$OUT/p0_report.txt"
