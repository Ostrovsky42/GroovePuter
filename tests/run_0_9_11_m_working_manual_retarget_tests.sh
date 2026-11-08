#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/m-working-manual-retarget"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"
cd "$ROOT"

set +e
python3 tests/test_0_9_11_m_working_manual_retarget_source_contract.py
SOURCE_STATUS=$?
set -e

pushd platform_sdl >/dev/null
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then
  echo "M-WORKING MW-L ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

set +e
"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tests/test_0_9_11_m_working_manual_retarget.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$BUILD/test_m_working_manual_retarget"
COMPILE_STATUS=$?
set -e
popd >/dev/null

HOST_STATUS=1
if [[ "$COMPILE_STATUS" -eq 0 ]]; then
  set +e
  "$BUILD/test_m_working_manual_retarget"
  HOST_STATUS=$?
  set -e
fi

if [[ "$SOURCE_STATUS" -ne 0 || "$COMPILE_STATUS" -ne 0 || "$HOST_STATUS" -ne 0 ]]; then
  echo "M-WORKING MW-L SUMMARY: source=$SOURCE_STATUS compile=$COMPILE_STATUS host=$HOST_STATUS" >&2
  exit 1
fi

echo "M-WORKING MW-L SUMMARY: PASS"
