#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/0918-lead/tests"
mkdir -p "$BUILD"
cd "$ROOT"
build_and_run() {
  local compiler="$1" suffix="$2"
  shift 2
  local sources=(src/generation/generation_context.cpp
    src/phrase/runtime_synth_events.cpp
    src/generation/composition/tonal_profile.cpp
    src/generation/roles/melodic_pitch_intent.cpp
    src/generation/roles/bass_rhythm.cpp
    src/generation/roles/bass_pitch_behavior.cpp
    src/generation/roles/chord_progression.cpp
    src/generation/tonal/tonal_projector.cpp
    src/generation/tonal/tonal_materializer.cpp
    src/generation/migration/tonal_pattern_adapter.cpp)
  for test in lead_phrase connected_bass tonal_profile_scope idea_variability; do
    "$compiler" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -Wno-unused-parameter -I. "$@" \
      "${sources[@]}" "tests/test_0918_${test}.cpp" -o "$BUILD/${test}_$suffix"
    "$BUILD/${test}_$suffix"
  done
}
build_and_run "${CXX:-g++}" gcc
if command -v clang++ >/dev/null 2>&1; then build_and_run clang++ clang; fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" build_and_run "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
echo '0.9.18 lead host matrix: OK'
