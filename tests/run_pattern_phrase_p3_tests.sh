#!/usr/bin/env bash
set -euo pipefail

# PATTERN/PHRASE P3 focused gate: bounded per-voice Phrase material and
# phrase-relative onset addressing, verified against a real MiniAcid built
# through the platform_sdl host target.
#
# The shared engine sources are compiled once into an archive and the three test
# binaries link against it. Compiling the whole source list once per test in a
# single g++ invocation peaks high enough to be OOM-killed on a 38 GB host.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_DIR="${ROOT_DIR}/platform_sdl"
BUILD_DIR="${ROOT_DIR}/build/pattern-phrase-p3"
OBJ_DIR="${BUILD_DIR}/obj"
mkdir -p "${OBJ_DIR}"

DEFAULT_SOURCES="$(make -C "${SDL_DIR}" -pn 2>/dev/null | sed -n 's/^SOURCES := //p' | head -n 1)"
if [[ -z "${DEFAULT_SOURCES}" ]]; then
  echo "P3 gate: failed to read platform_sdl SOURCES" >&2
  exit 1
fi

BASE_SOURCES=" ${DEFAULT_SOURCES} "
BASE_SOURCES="${BASE_SOURCES// sdl_main.cpp / }"

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
JOBS="$(nproc 2>/dev/null || echo 2)"
if (( JOBS > 4 )); then JOBS=4; fi

object_for() {
  # ../src/dsp/foo.cpp -> <OBJ_DIR>/src_dsp_foo.o
  local src="$1"
  local flat="${src#../}"
  flat="${flat//\//_}"
  printf '%s/%s.o' "${OBJ_DIR}" "${flat%.cpp}"
}

compile_one() {
  local src="$1"
  local obj
  obj="$(object_for "${src}")"
  if [[ -f "${obj}" && "${obj}" -nt "${src}" ]]; then
    return 0
  fi
  # shellcheck disable=SC2086
  g++ ${BASE_FLAGS} -O1 ${SDL_CFLAGS} ${SDL_GFX_CFLAGS} -c "${src}" -o "${obj}"
}
export -f compile_one object_for
export BASE_FLAGS SDL_CFLAGS SDL_GFX_CFLAGS OBJ_DIR

cd "${SDL_DIR}"

printf '%s\n' 'P3 gate: compiling shared engine objects'
printf '%s\n' ${BASE_SOURCES} | xargs -P "${JOBS}" -I{} bash -c 'compile_one "$@"' _ {}

OBJECTS=()
for src in ${BASE_SOURCES}; do
  OBJECTS+=("$(object_for "${src}")")
done

ARCHIVE="${BUILD_DIR}/libgrooveputer_p3.a"
rm -f "${ARCHIVE}"
ar rcs "${ARCHIVE}" "${OBJECTS[@]}"

TESTS=(
  test_p3_bounded_phrase_source_real
  test_p3_per_synth_phrase_cardinality
  test_p3_phrase_relative_onset
)

for test_name in "${TESTS[@]}"; do
  bin="${BUILD_DIR}/${test_name}"
  # shellcheck disable=SC2086
  g++ ${BASE_FLAGS} -O1 ${SDL_CFLAGS} ${SDL_GFX_CFLAGS} \
    "../tests/${test_name}.cpp" "${ARCHIVE}" \
    ${SDL_LIBS} ${SDL_GFX_LIBS} -o "${bin}"
  "${bin}"
done

printf '%s\n' 'PATTERN/PHRASE P3 focused gate: PASS'
