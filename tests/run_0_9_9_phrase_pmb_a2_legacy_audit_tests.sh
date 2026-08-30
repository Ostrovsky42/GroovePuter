#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-pmb-a2-legacy-audit"
BASE_SHA="03f633d2e431c75ec9b2f4402507b9253f785a74"
mkdir -p "$BUILD"

# PMB-A2 is research-only characterization: it must never carry a production
# semantic delta relative to the frozen PMB-A1 base.
git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/
echo "PMB-A2 frozen PMB-A1 src firewall: OK"

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
  ' "${ROOT}/platform_sdl/Makefile"
)
if [[ "${#SDL_SOURCES[@]}" -eq 0 ]]; then
  echo 'PMB-A2 ERROR: failed to resolve production SDL source set' >&2
  exit 2
fi

resolved=()
for source in "${SDL_SOURCES[@]}"; do
  resolved+=("${ROOT}/platform_sdl/${source}")
done

SDL_CFLAGS="$(sdl2-config --cflags 2>/dev/null || echo '-I/usr/include/SDL2 -D_THREAD_SAFE')"
SDL_LIBS="$(sdl2-config --libs 2>/dev/null || echo '-lSDL2')"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx 2>/dev/null || true)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx 2>/dev/null || echo '-lSDL2_gfx')"

CXXFLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Wno-c++20-extensions
  -Wno-unused-but-set-variable
  -I"${ROOT}"
  -I"${ROOT}/platform_sdl"
  -include "${ROOT}/platform_sdl/arduino_compat.h"
  ${SDL_CFLAGS}
  ${SDL_GFX_CFLAGS}
)
LDLIBS=(${SDL_LIBS} ${SDL_GFX_LIBS})
TEST="${ROOT}/tests/test_0_9_9_phrase_pmb_a2_legacy_audit.cpp"

build() {
  local compiler="$1"
  local output="$2"
  shift 2
  "$compiler" "${CXXFLAGS[@]}" "$@" "${resolved[@]}" "$TEST" "${LDLIBS[@]}" -o "$output"
}

build "${CXX:-g++}" "$BUILD/gcc"
"$BUILD/gcc" > "$BUILD/gcc-1.out"
"$BUILD/gcc" > "$BUILD/gcc-2.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/gcc-2.out"
cat "$BUILD/gcc-1.out"
echo "PMB-A2 deterministic repeat (whole-binary): OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/clang"
  "$BUILD/clang" > "$BUILD/clang.out"
  diff -u "$BUILD/gcc-1.out" "$BUILD/clang.out"
  echo "PMB-A2 Clang gate: OK"
fi

build "${CXX:-g++}" "$BUILD/asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/asan" > "$BUILD/asan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/asan.out"
echo "PMB-A2 ASan gate: OK"

build "${CXX:-g++}" "$BUILD/ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/ubsan" > "$BUILD/ubsan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/ubsan.out"
echo "PMB-A2 UBSan gate: OK"

git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/
echo "PMB-A2 final frozen PMB-A1 src firewall: OK"
echo "0.9.9-PHRASE-PMB-A2 legacy replay/preflight audit: PASS (research characterization only; no production delta)"
