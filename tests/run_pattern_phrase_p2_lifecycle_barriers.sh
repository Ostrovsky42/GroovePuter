#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_DIR="${ROOT_DIR}/platform_sdl"
BUILD_DIR="${ROOT_DIR}/build/pattern-phrase-p2-lifecycle-barriers"
mkdir -p "${BUILD_DIR}"

DEFAULT_SOURCES="$(make -C "${SDL_DIR}" -pn 2>/dev/null | sed -n 's/^SOURCES := //p' | head -n 1)"
if [[ -z "${DEFAULT_SOURCES}" ]]; then
  echo "P2 lifecycle barriers: failed to read platform_sdl SOURCES" >&2
  exit 1
fi

RUNTIME_SOURCES=" ${DEFAULT_SOURCES} "
RUNTIME_SOURCES="${RUNTIME_SOURCES// sdl_main.cpp / }"
RUNTIME_SOURCES="${RUNTIME_SOURCES} ../tests/test_pattern_phrase_p2_lifecycle_barriers.cpp"

SDL_CFLAGS="$(sdl2-config --cflags 2>/dev/null || true)"
SDL_LIBS="$(sdl2-config --libs 2>/dev/null || true)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx 2>/dev/null || true)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx 2>/dev/null || true)"
if [[ -z "${SDL_CFLAGS}" ]]; then
  SDL_CFLAGS="-I/usr/include/SDL2 -D_THREAD_SAFE"
  SDL_LIBS="-lSDL2"
fi
if [[ -z "${SDL_GFX_CFLAGS}" ]]; then
  SDL_GFX_CFLAGS=""
  SDL_GFX_LIBS="-lSDL2_gfx"
fi

BASE_FLAGS="-std=c++17 -I.. -I. -include bits/stdc++.h -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME"
BIN="${BUILD_DIR}/pattern_phrase_p2_lifecycle_barriers"
OUT="${BUILD_DIR}/behavior.out"

(
  cd "${SDL_DIR}"
  g++ ${BASE_FLAGS} -O1 ${SDL_CFLAGS} ${SDL_GFX_CFLAGS} \
    ${RUNTIME_SOURCES} ${SDL_LIBS} ${SDL_GFX_LIBS} -o "${BIN}"
)

set +e
"${BIN}" >"${OUT}" 2>&1
status=$?
set -e
cat "${OUT}"

if (( status != 0 )); then
  if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    {
      printf '%s\n' '### P2 lifecycle behavioral RED'
      printf '%s\n' '```text'
      cat "${OUT}"
      printf '%s\n' '```'
    } >> "${GITHUB_STEP_SUMMARY}"
  fi
  while IFS= read -r line; do
    [[ -n "${line}" ]] || continue
    printf '::error title=P2 lifecycle behavioral RED::%s\n' "${line}"
  done < "${OUT}"
  exit "${status}"
fi

printf '%s\n' 'PATTERN/PHRASE P2 lifecycle barriers behavior: PASS'
