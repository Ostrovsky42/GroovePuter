#!/usr/bin/env bash
# GENRE usability: tempo corridor on screen, the MATERIAL pointer, the
# not-applied notice. Full SDL source set, host only.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/genre-page-usability"
mkdir -p "$BUILD"

pushd "$ROOT/platform_sdl" >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
g++ -std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tests/test_genre_page_usability.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$BUILD/test"
popd >/dev/null

"$BUILD/test"
printf '%s\n' 'GENRE page usability: PASS'
