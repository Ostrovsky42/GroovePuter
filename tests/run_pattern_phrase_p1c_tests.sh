#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE_SHA="2860f99d254baa96e06d48b3a52d3e729c2e707a"
TMP="${TMPDIR:-/tmp}/grooveputer_pattern_phrase_p1c"
P0_BUILD="$ROOT/build/pattern-phrase-p1c-p0"
mkdir -p "$TMP" "$P0_BUILD"

git cat-file -e "${BASE_SHA}^{commit}"
if [[ "$(git merge-base HEAD "$BASE_SHA")" != "$BASE_SHA" ]]; then
  echo "P1C base verification failed" >&2
  exit 1
fi
printf '%s\n' 'P1C current dev base verified: PASS'

python3 tests/test_pattern_phrase_p0_source_contract.py

SDL_DIR="$ROOT/platform_sdl"
DEFAULT_SOURCES="$(make -C "$SDL_DIR" -pn 2>/dev/null | sed -n 's/^SOURCES := //p' | head -n 1)"
if [[ -z "$DEFAULT_SOURCES" ]]; then
  echo "P1C/P0: failed to read platform_sdl SOURCES" >&2
  exit 1
fi

RUNTIME_SOURCES=" ${DEFAULT_SOURCES} "
RUNTIME_SOURCES="${RUNTIME_SOURCES// sdl_main.cpp / }"
RUNTIME_SOURCES="${RUNTIME_SOURCES} ../tests/test_pattern_phrase_p0_runtime.cpp ../src/midi/usb_midi_output.cpp"

SDL_CFLAGS="$(sdl2-config --cflags 2>/dev/null || true)"
SDL_LIBS="$(sdl2-config --libs 2>/dev/null || true)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx 2>/dev/null || true)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx 2>/dev/null || true)"
if [[ -z "$SDL_CFLAGS" ]]; then
  SDL_CFLAGS="-I/usr/include/SDL2 -D_THREAD_SAFE"
  SDL_LIBS="-lSDL2"
fi
if [[ -z "$SDL_GFX_CFLAGS" ]]; then
  SDL_GFX_LIBS="-lSDL2_gfx"
fi

P0_FLAGS="-std=c++17 -I.. -I. -include bits/stdc++.h -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME"
(
  cd "$SDL_DIR"
  g++ $P0_FLAGS -O1 $SDL_CFLAGS $SDL_GFX_CFLAGS \
    $RUNTIME_SOURCES $SDL_LIBS $SDL_GFX_LIBS -o "$P0_BUILD/p0-runtime"
  "$P0_BUILD/p0-runtime"
) | tee "$P0_BUILD/p0-runtime.out"
printf '%s\n' 'P1C P0 legacy scheduler characterization: PASS'

python3 tests/test_pattern_phrase_p1c_source_contract.py

CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror -I.)
TEST_SRC=tests/test_pattern_phrase_p1c_runtime_events.cpp
OWNER_SRC=()
if [[ -f src/phrase/runtime_synth_events.cpp ]]; then
  OWNER_SRC+=(src/phrase/runtime_synth_events.cpp)
fi

build_and_run() {
  local cxx="$1"
  local output="$2"
  shift 2
  "$cxx" "${CXXFLAGS[@]}" "$@" "${OWNER_SRC[@]}" "$TEST_SRC" -o "$output"
  "$output"
}

build_and_run g++ "$TMP/p1c-gcc" | tee "$TMP/p1c-gcc.out"
"$TMP/p1c-gcc" > "$TMP/p1c-gcc-repeat.out"
diff -u "$TMP/p1c-gcc.out" "$TMP/p1c-gcc-repeat.out"
printf '%s\n' 'P1C deterministic GCC repeat: PASS'

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "$TMP/p1c-clang" > "$TMP/p1c-clang.out"
  diff -u "$TMP/p1c-gcc.out" "$TMP/p1c-clang.out"
  printf '%s\n' 'P1C Clang parity: PASS'
fi

build_and_run g++ "$TMP/p1c-asan" \
  -fsanitize=address -fno-omit-frame-pointer > /dev/null
printf '%s\n' 'P1C ASan: PASS'

build_and_run g++ "$TMP/p1c-ubsan" \
  -fsanitize=undefined -fno-sanitize-recover=undefined > /dev/null
printf '%s\n' 'P1C UBSan: PASS'

python3 tests/test_pattern_phrase_p1c_source_contract.py
git diff --check "$BASE_SHA"...HEAD
printf '%s\n' 'P1C scheduler source firewall: PASS'
printf '%s\n' 'P1C Performance firewall: PASS'
printf '%s\n' 'PATTERN/PHRASE P1C focused gate: PASS'
