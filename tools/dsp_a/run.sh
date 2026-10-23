#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:?usage: tools/dsp_a/run.sh OUTPUT_DIR [--sanitize]}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
BUILD="$ROOT/build/dsp-a"
mkdir -p "$BUILD"
FLAGS=(-std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-c++20-extensions -I"$ROOT" -include "$ROOT/platform_sdl/arduino_compat.h")
SUFFIX=release
if [[ "${2:-}" == --sanitize ]]; then
  FLAGS+=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  SUFFIX=sanitize
else FLAGS+=(-O2); fi
SOURCES=(swappable_synth_voice mini_tb303 filter audio_wavetables sh101_synth_voice wave_morph_synth_voice ay_synth_voice sid_synth sid_synth_voice sn76489_synth_voice)
FILES=()
for S in "${SOURCES[@]}"; do FILES+=("$ROOT/src/dsp/$S.cpp"); done
"${CXX:-g++}" "${FLAGS[@]}" "$ROOT/tools/dsp_a/render.cpp" "${FILES[@]}" -o "$BUILD/render-$SUFFIX"
printf 'source=%s\nbase=64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f\nsample_rate=22050\ncompiler=%s\n' "$(git -C "$ROOT" rev-parse HEAD)" "${CXX:-g++}" > "$OUT/provenance.txt"
git -C "$ROOT" diff --binary HEAD > "$OUT/source.patch"
sha256sum "${FILES[@]}" "$ROOT/tools/dsp_a/render.cpp" > "$OUT/source-files.sha256"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/render-$SUFFIX" "$OUT"
