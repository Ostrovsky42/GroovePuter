#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/c0-ui-input-ownership"
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
  echo 'C0_UI_INPUT ERROR: failed to resolve SDL source set' >&2
  exit 2
fi

"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $SDL_CFLAGS $SDL_GFX_CFLAGS \
  "${SDL_SOURCES[@]}" ../tests/test_c0_ui_input_ownership.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/test_c0_ui_input_ownership"
"$BUILD/test_c0_ui_input_ownership"
popd >/dev/null

# Existing RED/contract: Phrase editor must own physical/scancode L and G,
# including safe/rejected shrink behavior.
bash "$ROOT/tests/run_ui_pattern_phrase_hardware_controls_tests.sh"

printf '%s\n' 'C0 UI input ownership focused gate: PASS'
