#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_DIR="${ROOT_DIR}/platform_sdl"
BUILD_DIR="${ROOT_DIR}/build/pattern-phrase-p0"
BASE_SHA="6694876edff654bc0e14cafd3181c7ff2ff5060e"
mkdir -p "${BUILD_DIR}"

cd "${ROOT_DIR}"
git cat-file -e "${BASE_SHA}^{commit}"
git diff --exit-code "${BASE_SHA}" -- src/
python3 tests/test_pattern_phrase_p0_source_contract.py

DEFAULT_SOURCES="$(make -C "${SDL_DIR}" -pn 2>/dev/null | sed -n 's/^SOURCES := //p' | head -n 1)"
if [[ -z "${DEFAULT_SOURCES}" ]]; then
  echo "P0: failed to read platform_sdl SOURCES" >&2
  exit 1
fi

RUNTIME_SOURCES=" ${DEFAULT_SOURCES} "
RUNTIME_SOURCES="${RUNTIME_SOURCES// sdl_main.cpp / }"
RUNTIME_SOURCES="${RUNTIME_SOURCES} ../tests/test_pattern_phrase_p0_runtime.cpp ../src/midi/usb_midi_output.cpp"

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

build_and_run() {
  local cxx="$1"
  local output="$2"
  local extra_flags="$3"
  local log="$4"
  (
    cd "${SDL_DIR}"
    ${cxx} ${BASE_FLAGS} ${extra_flags} ${SDL_CFLAGS} ${SDL_GFX_CFLAGS} \
      ${RUNTIME_SOURCES} ${SDL_LIBS} ${SDL_GFX_LIBS} -o "${output}"
    "${output}"
  ) > "${log}"
}

GCC_BIN="${BUILD_DIR}/pattern_phrase_p0_gcc"
GCC_LOG1="${BUILD_DIR}/gcc-run1.txt"
GCC_LOG2="${BUILD_DIR}/gcc-run2.txt"
build_and_run g++ "${GCC_BIN}" "-O1" "${GCC_LOG1}"
"${GCC_BIN}" > "${GCC_LOG2}"
diff -u "${GCC_LOG1}" "${GCC_LOG2}"
cat "${GCC_LOG1}"

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/pattern_phrase_p0_clang" "-O1" "${BUILD_DIR}/clang.txt"
  diff -u "${GCC_LOG1}" "${BUILD_DIR}/clang.txt"
fi

build_and_run g++ "${BUILD_DIR}/pattern_phrase_p0_asan" \
  "-O1 -g -fsanitize=address -fno-omit-frame-pointer" \
  "${BUILD_DIR}/asan.txt"
diff -u "${GCC_LOG1}" "${BUILD_DIR}/asan.txt"

build_and_run g++ "${BUILD_DIR}/pattern_phrase_p0_ubsan" \
  "-O1 -g -fsanitize=undefined -fno-omit-frame-pointer" \
  "${BUILD_DIR}/ubsan.txt"
diff -u "${GCC_LOG1}" "${BUILD_DIR}/ubsan.txt"

git diff --exit-code "${BASE_SHA}" -- src/
echo "PATTERN/PHRASE P0 characterization: PASS"
