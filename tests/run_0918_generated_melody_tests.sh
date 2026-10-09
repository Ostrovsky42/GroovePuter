#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/0918-generated-melody"
mkdir -p "$BUILD"

CXX="${CXX:-g++}"

printf '%s\n' '=== 0.9.18 G into a Melody ==='
pushd "$ROOT/platform_sdl" >/dev/null

SDL_CFLAGS="$(sdl2-config --cflags)"
SDL_LIBS="$(sdl2-config --libs)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx)"

mapfile -t SDL_SOURCES < <(
  awk '
    /^SOURCES :=/ { capture = 1; next }
    capture && /^[^[:space:]]/ { capture = 0 }
    capture {
      line = $0
      sub(/^[[:space:]]+/, "", line)
      sub(/[[:space:]]*\\[[:space:]]*$/, "", line)
      count = split(line, parts, /[[:space:]]+/)
      for (i = 1; i <= count; ++i) {
        if (parts[i] != "" && parts[i] != "sdl_main.cpp") print parts[i]
      }
    }
  ' Makefile
)

if [[ "${#SDL_SOURCES[@]}" -eq 0 ]]; then
  echo '0918-MELODY ERROR: failed to resolve production SDL source set' >&2
  exit 2
fi

CXXFLAGS_COMMON=(
  -std=c++17 -Wall -Wextra -Wno-c++20-extensions
  -I.. -I. -include arduino_compat.h
  $SDL_CFLAGS $SDL_GFX_CFLAGS
)

for test in generated_melody dnb_genre; do
  "$CXX" "${CXXFLAGS_COMMON[@]}" -O2 \
    "${SDL_SOURCES[@]}" "../tests/test_0918_${test}.cpp" \
    $SDL_LIBS $SDL_GFX_LIBS -o "$BUILD/${test}_gcc"
  "$BUILD/${test}_gcc"

  "$CXX" "${CXXFLAGS_COMMON[@]}" -O1 -g -fno-omit-frame-pointer \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    "${SDL_SOURCES[@]}" "../tests/test_0918_${test}.cpp" \
    $SDL_LIBS $SDL_GFX_LIBS -o "$BUILD/${test}_sanitize"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/${test}_sanitize"
done

popd >/dev/null
echo '0.9.18 generated melody gate: OK'
