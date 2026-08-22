#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/e0-temporal-carrier"
mkdir -p "$BUILD"

CXX="${CXX:-g++}"
COMMON_FLAGS=(
  -std=c++17
  -O2
  -Wall
  -Wextra
  -Werror
  -Wno-c++20-extensions
  -I"$ROOT"
  -I"$ROOT/platform_sdl"
  -include "$ROOT/platform_sdl/arduino_compat.h"
)

printf '%s\n' '=== E0 ABI / memory-category probe ==='
"$CXX" "${COMMON_FLAGS[@]}" \
  "$ROOT/tests/e0_temporal_carrier_sizes.cpp" \
  -o "$BUILD/e0_temporal_carrier_sizes"
"$BUILD/e0_temporal_carrier_sizes"

printf '%s\n' '=== E0 ownership / reset source audit ==='
python3 "$ROOT/tests/e0_temporal_carrier_source_audit.py"

printf '%s\n' '=== E0 full GeneratedPhraseSong::prepare host benchmark ==='
pushd "$ROOT/platform_sdl" >/dev/null

SDL_CFLAGS="$(sdl2-config --cflags)"
SDL_LIBS="$(sdl2-config --libs)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx)"

mapfile -t SDL_SOURCES < <(
  awk '
    /^SOURCES :=/ { capture = 1; next }
    capture && /^ROOT :=/ { capture = 0 }
    capture {
      line = $0
      sub(/^[[:space:]]+/, "", line)
      sub(/[[:space:]]*\\[[:space:]]*$/, "", line)
      if (line != "" && line != "sdl_main.cpp") print line
    }
  ' Makefile
)

# The benchmark deliberately compiles the exact production SDL source set.
# Existing production warnings are not E0 failures, so keep diagnostics visible
# but do not promote inherited warnings to errors here. The ABI probe above
# remains strict because it compiles only E0 research code.
"$CXX" \
  -std=c++17 -O2 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $SDL_CFLAGS $SDL_GFX_CFLAGS \
  "${SDL_SOURCES[@]}" e0_prepare_benchmark.cpp \
  $SDL_LIBS $SDL_GFX_LIBS \
  -o "$BUILD/e0_prepare_benchmark"

"$BUILD/e0_prepare_benchmark"

printf '%s\n' '=== E0 static stack-frame instrumentation ==='
rm -f "$BUILD/e0_prepare_benchmark.su" "$BUILD/e0_prepare_benchmark.o"
"$CXX" \
  -std=c++17 -O2 -fno-inline -fstack-usage \
  -I.. -I. -include arduino_compat.h \
  -c e0_prepare_benchmark.cpp \
  -o "$BUILD/e0_prepare_benchmark.o"

if [[ -f "$BUILD/e0_prepare_benchmark.su" ]]; then
  grep -E 'GeneratedPhraseSong::prepare|benchmarkCase|runPrepare|main' \
    "$BUILD/e0_prepare_benchmark.su" || true
else
  echo 'E0_STACK static .su output not emitted by compiler'
fi

popd >/dev/null

echo '0.9.9-E0 temporal material carrier audit harness: PASS'
