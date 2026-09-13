#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/c1-full"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"
cd "$ROOT"

pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then
  echo "C1 ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

compile_run() {
  local source="$1"
  local name="$2"
  "$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
    -I.. -I. -include arduino_compat.h \
    $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
    "${SRCS[@]}" "../tests/${source}" \
    $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
    -o "$BUILD/${name}"
  "$BUILD/${name}"
}

compile_run test_0_9_11_c1_identity_working.cpp c1_identity_working
compile_run test_0_9_11_c1_manual_working.cpp c1_manual_working
compile_run test_0_9_11_c1_manual_retarget.cpp c1_manual_retarget
popd >/dev/null

"$CXX" -std=c++20 -Wall -Wextra -Werror -I. -Iplatform_sdl \
  tests/test_0_9_11_c1_identity_promotion.cpp -o "$BUILD/c1_identity_promotion"
"$BUILD/c1_identity_promotion"

echo "C1 FULL CONTRACTS: PASS"
