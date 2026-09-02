#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/e0a-temporal-coordinates"
mkdir -p "$BUILD"

CXX="${CXX:-g++}"

printf '%s\n' '=== E0a source ownership / lifecycle regressions ==='
python3 "$ROOT/tests/test_0_9_9_e0a_source_regressions.py"

printf '%s\n' '=== E0a temporal coordinate runtime characterization ==='
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
  echo 'E0A_TEST ERROR: failed to resolve production SDL source set' >&2
  exit 2
fi

# These integration binaries compile the exact production SDL source set.
# Existing production warnings remain visible but are not promoted to errors;
# focused E0a source/contract failures still fail the harness.
"$CXX" \
  -std=c++17 -O2 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $SDL_CFLAGS $SDL_GFX_CFLAGS \
  "${SDL_SOURCES[@]}" ../tests/test_0_9_9_e0a_temporal_coordinates.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/test_0_9_9_e0a_temporal_coordinates"

"$BUILD/test_0_9_9_e0a_temporal_coordinates"

printf '%s\n' '=== E0a full GeneratedPhraseSong::prepare host benchmark ==='
"$CXX" \
  -std=c++17 -O2 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $SDL_CFLAGS $SDL_GFX_CFLAGS \
  "${SDL_SOURCES[@]}" e0a_prepare_benchmark.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/e0a_prepare_benchmark"

"$BUILD/e0a_prepare_benchmark"

printf '%s\n' '=== E0a host GCC static stack-frame instrumentation ==='
rm -f "$BUILD/e0a_prepare_benchmark.su" "$BUILD/e0a_prepare_benchmark.o"
"$CXX" \
  -std=c++17 -O2 -fno-inline -fstack-usage \
  -I.. -I. -include arduino_compat.h \
  -c e0a_prepare_benchmark.cpp \
  -o "$BUILD/e0a_prepare_benchmark.o"

if [[ -f "$BUILD/e0a_prepare_benchmark.su" ]]; then
  grep -E 'GeneratedPhraseSong::prepare|benchmarkCase|runPrepare|main' \
    "$BUILD/e0a_prepare_benchmark.su" || true
else
  echo 'E0A_STACK static .su output not emitted by compiler'
fi

popd >/dev/null

printf '%s\n' 'E0A_STACK_CAVEAT host GCC .su != ESP32-S3 task HWM'
printf '%s\n' 'E0A_ESP32_PREPARE_HWM NOT MEASURED'
printf '%s\n' '0.9.9-E0a temporal coordinates: PASS'
