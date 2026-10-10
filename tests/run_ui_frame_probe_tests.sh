#!/usr/bin/env bash
# 0.9.19 UI frame probe: stage accumulator, audio busy clock, and the audio
# mutation gate (with its wait/hold counters).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/ui-frame-probe"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"
cd "$ROOT"

"$CXX" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I. \
  tests/test_ui_frame_probe.cpp -o "$BUILD/test_ui_frame_probe"
"$BUILD/test_ui_frame_probe"

"$CXX" -std=c++17 -Wall -Wextra -Werror -pthread -I. \
  tests/test_audio_mutation_gate.cpp -o "$BUILD/test_audio_mutation_gate"
"$BUILD/test_audio_mutation_gate"
echo "Audio mutation gate (pause at block boundary, I/O window, wait/hold counters): PASS"
echo "0.9.19 UI frame probe: GREEN"
