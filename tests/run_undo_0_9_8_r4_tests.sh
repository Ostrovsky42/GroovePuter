#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/undo-0.9.8-r4"
mkdir -p "$BUILD_DIR"

# R4 is additive on the accepted R3 Pattern ownership slice.
bash "$ROOT/tests/run_undo_0_9_8_r3_tests.sh"
python3 "$ROOT/tests/test_song_phrase_safe_editing_0_9_8_r4_source_regressions.py"

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tests/test_song_phrase_safe_editing_0_9_8_r4.cpp" \
  -o "$BUILD_DIR/test_song_phrase_safe_editing_0_9_8_r4"

"$BUILD_DIR/test_song_phrase_safe_editing_0_9_8_r4"
echo "0.9.8 R4 Song/Phrase safe-editing migration: PASS"
