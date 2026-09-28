#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/0-9-14-d1b1"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

cd "$ROOT"
trap 'rm -rf "$ROOT/patterns" "$ROOT/platform_sdl/patterns" "$ROOT/projects" "$ROOT/grooveputer_scene_name.txt"' EXIT

echo "== D1-B1 focused runtime test =="
pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)

if (( ${#SRCS[@]} == 0 )); then
  echo "D1-B1 ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 -DGROOVEPUTER_M1_TEST_PROBE \
  "${SRCS[@]}" ../tests/test_0_9_14_d1b1_pitch_class_origin.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$BUILD/test_0_9_14_d1b1_pitch_class_origin"
popd >/dev/null

"$BUILD/test_0_9_14_d1b1_pitch_class_origin"

echo "== D1-B1 source regressions =="
python3 "$ROOT/tests/test_0_9_14_d1b1_source_regressions.py"

echo "== D1-B regression gate =="
bash "$ROOT/tests/run_0_9_14_d1b_tests.sh"

echo "0.9.14 D1-B1: GREEN"
