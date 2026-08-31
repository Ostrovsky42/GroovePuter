#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/phrase-pmb-p1"
mkdir -p "$BUILD"

CXX="${CXX:-g++}"

printf '%s\n' '=== PMB-P1 bounded PREPARE/COMMIT runtime characterization ==='
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
      if (line != "" && line != "sdl_main.cpp") print line
    }
  ' Makefile
)

if [[ "${#SDL_SOURCES[@]}" -eq 0 ]]; then
  echo 'PMB-P1_TEST ERROR: failed to resolve production SDL source set' >&2
  exit 2
fi

CXXFLAGS_COMMON=(
  -std=c++17 -Wall -Wextra -Wno-c++20-extensions
  -I.. -I. -include arduino_compat.h
  $SDL_CFLAGS $SDL_GFX_CFLAGS
)

# Existing production warnings remain visible but are not promoted to errors;
# focused PMB-P1 source/contract failures still fail the harness.
"$CXX" \
  "${CXXFLAGS_COMMON[@]}" -O2 \
  "${SDL_SOURCES[@]}" ../tests/test_0_9_9_phrase_pmb_p1_bounded_prepare_commit.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/gcc"
"$BUILD/gcc"
echo "PMB-P1 focused bounded PREPARE/COMMIT (gcc): OK"

"$CXX" \
  "${CXXFLAGS_COMMON[@]}" -O1 -g -fno-omit-frame-pointer -fsanitize=address \
  "${SDL_SOURCES[@]}" ../tests/test_0_9_9_phrase_pmb_p1_bounded_prepare_commit.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/asan"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/asan"
echo "PMB-P1 ASan gate: OK"

"$CXX" \
  "${CXXFLAGS_COMMON[@]}" -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined \
  "${SDL_SOURCES[@]}" ../tests/test_0_9_9_phrase_pmb_p1_bounded_prepare_commit.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/ubsan"
"$BUILD/ubsan"
echo "PMB-P1 UBSan gate: OK"

popd >/dev/null

echo "PMB-P1 focused bounded PREPARE/COMMIT gate: OK"
