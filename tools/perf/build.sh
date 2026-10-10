#!/usr/bin/env bash
# Build a tools/perf tool against the full SDL source set (host only).
# Usage: tools/perf/build.sh <tool.cpp> <output-binary> [gprof]
#   render_bench.cpp     whole engine, busy scene: host us/block per engine/mute
#   component_bench.cpp  src/perf/render_bench.h: interleaved vs blockwise
#   gprof: build with -pg; run the binary, then `gprof <binary> gmon.out`.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
PROFILE=()
if [[ "${3:-}" == "gprof" ]]; then PROFILE=(-pg -fno-omit-frame-pointer -fno-inline-functions); fi
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 } c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l); n = split(l, p, /[[:space:]]+/); for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile)
g++ -std=c++17 -w -I.. -I. -include arduino_compat.h $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) \
  -O2 "${PROFILE[@]}" "${SRCS[@]}" "$SRC" \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$OUT"
