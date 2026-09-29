#!/usr/bin/env bash
# Render A -> published DEVELOP -> published BREAK (real generate + generateCycle, mix C). Host only.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/p0/product_cycle}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
"$CXX" -std=c++17 -w -Wno-c++20-extensions -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tools/m0/p0_cycle_render.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$OUT/p0_cycle_render"
cd "$ROOT"
trap 'rm -rf "$ROOT/patterns" "$ROOT/platform_sdl/patterns" "$ROOT/projects" "$ROOT/grooveputer_scene_name.txt"' EXIT
P0_CYCLE_OUT="$OUT" "$OUT/p0_cycle_render" 2>&1 | grep -E "ok|rendered|FAIL"
if command -v ffmpeg >/dev/null; then
  for f in "$OUT"/*.wav; do ffmpeg -y -loglevel error -i "$f" -codec:a libmp3lame -b:a 128k "${f%.wav}.mp3"; done
fi
