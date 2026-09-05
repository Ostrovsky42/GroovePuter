#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

python3 tests/test_p3_u1_source_projection.py

FAST_BUILD_DIR="${ROOT_DIR}/build/pattern-phrase-p3-u1-fast"
mkdir -p "${FAST_BUILD_DIR}"

g++ -std=c++17 -I. tests/test_p3_u1_phrase_projection.cpp \
  -o "${FAST_BUILD_DIR}/test_p3_u1_phrase_projection"
"${FAST_BUILD_DIR}/test_p3_u1_phrase_projection"

# Build the inherited real MiniAcid archive first. The mutation characterization
# then links against the same host engine objects instead of using a test double.
bash tests/run_pattern_phrase_p3_tests.sh

SDL_CFLAGS="$(sdl2-config --cflags 2>/dev/null || true)"
SDL_LIBS="$(sdl2-config --libs 2>/dev/null || true)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx 2>/dev/null || true)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx 2>/dev/null || true)"
if [[ -z "${SDL_CFLAGS}" ]]; then
  SDL_CFLAGS="-I/usr/include/SDL2 -D_THREAD_SAFE"
  SDL_LIBS="-lSDL2"
fi
if [[ -z "${SDL_GFX_LIBS}" ]]; then
  SDL_GFX_CFLAGS=""
  SDL_GFX_LIBS="-lSDL2_gfx"
fi

BASE_FLAGS="-std=c++17 -I.. -I. -include bits/stdc++.h -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME"
ARCHIVE="${ROOT_DIR}/build/pattern-phrase-p3/libgrooveputer_p3.a"

cd "${ROOT_DIR}/platform_sdl"
# shellcheck disable=SC2086
g++ ${BASE_FLAGS} -O1 ${SDL_CFLAGS} ${SDL_GFX_CFLAGS} \
  "../tests/test_p3_u1_phrase_mutation.cpp" "${ARCHIVE}" \
  ${SDL_LIBS} ${SDL_GFX_LIBS} \
  -o "${FAST_BUILD_DIR}/test_p3_u1_phrase_mutation"
"${FAST_BUILD_DIR}/test_p3_u1_phrase_mutation"

printf '%s\n' 'PATTERN/PHRASE P3-U1 focused gate: PASS'
