#!/usr/bin/env bash
# Build one generator probe against the full SDL source set (host only).
# Usage: tools/gen_probe/build.sh <probe.cpp> <output-binary>
#   genprobe.cpp   [presses] [print] [level]  G on STEPS, 16 genres x N presses, stats table
#   migprobe.cpp   same + "#MIG" lines: lead before/after migrateStrongRhythmMaterial
#   matprobe.cpp   [print]                    G on MATERIAL (4-bar TAKE) + D, per genre
#   leadrender.cpp <out-dir> <tag>            Acid/Synthwave/House, 2 takes, 8-bar WAV each
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
cd "$ROOT/platform_sdl"
mapfile -t SRCS < <(awk '/^SOURCES :=/ { c = 1; next } c && /^[^[:space:]]/ { c = 0 } c { l = $0; sub(/^[[:space:]]+/, "", l); sub(/[[:space:]]*\\[[:space:]]*$/, "", l); n = split(l, p, /[[:space:]]+/); for (i = 1; i <= n; i++) if (p[i] != "" && p[i] != "sdl_main.cpp") print p[i] }' Makefile)
g++ -std=c++17 -w -I.. -I. -include arduino_compat.h $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 "${SRCS[@]}" "$SRC" $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "$OUT"
