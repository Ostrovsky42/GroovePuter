#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BUILD="$ROOT/build/host-tests/m2c"; mkdir -p "$BUILD"
pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
g++ -std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tests/test_page_material_kind_migration.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$BUILD/test"
popd >/dev/null
cd platform_sdl && "$BUILD/test"
printf '%s\n' 'M2c page material kind gate: PASS'
