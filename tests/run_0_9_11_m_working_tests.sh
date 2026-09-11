#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/m-working"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

cd "$ROOT"
python3 tests/test_0_9_11_m_working_source_contract.py

pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)

if (( ${#SRCS[@]} == 0 )); then
  echo "M-WORKING ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

# Match the established SDL characterization runners: warnings stay visible,
# but pre-existing whole-repository warnings are not promoted into unrelated
# M-WORKING failures.
"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tests/test_0_9_11_m_working.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$BUILD/test_m_working"
popd >/dev/null

set +e
"$BUILD/test_m_working"
STATUS=$?
set -e

case "$STATUS" in
  0)
    echo "M-WORKING: ALL CONTRACTS GREEN"
    ;;
  1)
    echo "M-WORKING: TRUE RED WITNESSES PROVEN"
    ;;
  2)
    echo "M-WORKING: STOP GATE TRIPPED" >&2
    ;;
  *)
    echo "M-WORKING: unexpected test status $STATUS" >&2
    ;;
esac

exit "$STATUS"
