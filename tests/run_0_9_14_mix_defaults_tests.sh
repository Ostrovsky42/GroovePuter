#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/0-9-14-mix-defaults"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

cd "$ROOT"
source "$ROOT/tests/lib/isolated_workdir.sh"
isolated_init

echo "== mix defaults: product engine path =="
pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then
  echo "mix defaults ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tests/test_0_9_14_mix_defaults.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$BUILD/test_0_9_14_mix_defaults"
popd >/dev/null

isolated_run "$BUILD/test_0_9_14_mix_defaults"
echo "0.9.14 mix defaults: GREEN"
