#!/usr/bin/env bash
# Build and run the M0-A host measurement tool (never linked into firmware).
# Usage: tools/m0/build_m0a.sh [output-dir]      (default: build/m0a)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/m0a}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then echo "M0-A ERROR: failed to resolve SDL source set" >&2; exit 3; fi
"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions -Wno-unused-parameter \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tools/m0/m0a_corpus.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$OUT/m0a_corpus"
cd "$ROOT"
# The tool writes project pages into its working directory: keep that out of the repository.
OUT="$(cd "$OUT" && pwd)"
GP_TOOL_WORK="$(mktemp -d "${TMPDIR:-/tmp}/gp-tool-work.XXXXXX")"
trap 'rm -rf "$GP_TOOL_WORK"' EXIT
(cd "$GP_TOOL_WORK" && M0_OUT="$OUT" "$OUT/m0a_corpus" | tee "$OUT/m0a_report.txt")
