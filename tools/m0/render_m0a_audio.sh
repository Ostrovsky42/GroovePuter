#!/usr/bin/env bash
# Render the M0-A listening scenarios to audio with the REAL engine (host only).
# Usage: tools/m0/render_m0a_audio.sh [output-dir]     (default: build/m0a/audio)
# Produces WAV per scenario, manifest.tsv (cue tables) and, if ffmpeg exists, MP3s.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/m0a/audio}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(
  awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 }
       c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l);
           n = split(l, p, /[[:space:]]+/);
           for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile
)
"$CXX" -std=c++17 -Wall -Wextra -Wno-c++20-extensions -Wno-unused-parameter -Wno-unused-function \
  -I.. -I. -include arduino_compat.h \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
  "${SRCS[@]}" ../tools/m0/m0a_render.cpp \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) \
  -o "$OUT/m0a_render"
cd "$ROOT"
M0_AUDIO_OUT="$OUT" "$OUT/m0a_render" | tee "$OUT/render_report.txt"
if command -v ffmpeg >/dev/null; then
  for f in "$OUT"/*.wav; do
    ffmpeg -y -loglevel error -i "$f" -codec:a libmp3lame -b:a 128k "${f%.wav}.mp3"
  done
fi
