#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/pattern-tie-ui"
mkdir -p "$BUILD"
pushd "$ROOT/platform_sdl" >/dev/null
SDL_CFLAGS="$(sdl2-config --cflags)"; SDL_LIBS="$(sdl2-config --libs)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx)"; SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx)"
mapfile -t SDL_SOURCES < <(
  awk '/^SOURCES :=/ { capture = 1; next }
       capture && /^[^[:space:]]/ { capture = 0 }
       capture { line = $0; sub(/^[[:space:]]+/, "", line);
                 sub(/[[:space:]]*\\[[:space:]]*$/, "", line);
                 n = split(line, parts, /[[:space:]]+/);
                 for (i = 1; i <= n; i++)
                   if (parts[i] != "" && parts[i] != "sdl_main.cpp") print parts[i] }' Makefile
)
g++ -std=c++17 -Wno-c++20-extensions -w -I.. -I. -include arduino_compat.h \
  $SDL_CFLAGS $SDL_GFX_CFLAGS -O0 \
  "${SDL_SOURCES[@]}" ../tests/test_pattern_tie_ui.cpp \
  $SDL_LIBS $SDL_GFX_LIBS -o "$BUILD/test"
popd >/dev/null
"$BUILD/test"
