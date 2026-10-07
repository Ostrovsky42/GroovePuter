#!/usr/bin/env bash
# UI tests that need the full SDL source set (real MiniAcidDisplay + engine):
# TEMPO panel and the MIDI IN Space mute, and Alt+N new empty Melody.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/ui-navigation"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
if (( ${#SRCS[@]} == 0 )); then
  echo "UI NAVIGATION ERROR: failed to resolve SDL source set" >&2
  exit 3
fi

for test in test_tempo_sync_ui test_melody_new_empty_ui; do
  "$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions \
    -I.. -I. -include arduino_compat.h \
    $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
    "${SRCS[@]}" "../tests/${test}.cpp" \
    $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
    -o "$BUILD/$test"
done

# The tests create project files in their working directory: keep that out
# of the repository.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/gp-ui-navigation.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"
"$BUILD/test_tempo_sync_ui" > "$BUILD/test_tempo_sync_ui.out"
tail -1 "$BUILD/test_tempo_sync_ui.out"
"$BUILD/test_melody_new_empty_ui" > /dev/null
echo "UI navigation: Alt+N new empty Melody PASS"
