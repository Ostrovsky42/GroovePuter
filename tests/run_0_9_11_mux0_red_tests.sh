#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/mux0"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

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
      n = split(line, parts, /[[:space:]]+/)
      for (i = 1; i <= n; i++)
        if (parts[i] != "" && parts[i] != "sdl_main.cpp") print parts[i]
    }
  ' Makefile
)

if [[ "${#SDL_SOURCES[@]}" -eq 0 ]]; then
  echo 'M_UX0 ERROR: failed to resolve SDL source set' >&2
  exit 2
fi

"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $SDL_CFLAGS $SDL_GFX_CFLAGS \
  "${SDL_SOURCES[@]}" ../tests/test_0_9_11_mux0_red.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/test_mux0_red"
popd >/dev/null

set +e
"$BUILD/test_mux0_red"
EXIT_CODE=$?
set -e

if [[ $EXIT_CODE -ne 0 ]]; then
  echo "M-UX0: RED WITNESSES PROVEN (exit code $EXIT_CODE)"
  exit $EXIT_CODE
fi

echo "M-UX0: ALL GREEN (UNEXPECTED FOR RED PASS)"
exit 0
