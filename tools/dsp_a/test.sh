#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/build/dsp-a/quality-tests"
mkdir -p "$BUILD"
FLAGS=(-std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -I"$ROOT")
SOURCES=("$ROOT/src/dsp/swappable_synth_voice.cpp" "$ROOT/src/dsp/ay_synth_voice.cpp" "$ROOT/src/dsp/sh101_synth_voice.cpp" "$ROOT/src/dsp/sid_synth.cpp" "$ROOT/src/dsp/sid_synth_voice.cpp" "$ROOT/src/dsp/sn76489_synth_voice.cpp" "$ROOT/src/dsp/mini_tb303.cpp" "$ROOT/src/dsp/filter.cpp" "$ROOT/src/dsp/audio_wavetables.cpp" "$ROOT/src/dsp/wave_morph_synth_voice.cpp" "$ROOT/src/dsp/sound_quality_presets.cpp")
for MODE in release sanitize; do
  EXTRA=(-O2)
  if [[ "$MODE" == sanitize ]]; then EXTRA=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer); fi
  "${CXX:-g++}" "${FLAGS[@]}" "${EXTRA[@]}" "$ROOT/tests/test_dsp_a_quality.cpp" "${SOURCES[@]}" -o "$BUILD/quality-$MODE"
  for CASE in filter tb-retrigger wave-retrigger presets state automation; do ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/quality-$MODE" "$CASE"; done
  "${CXX:-g++}" "${FLAGS[@]}" "${EXTRA[@]}" "$ROOT/tests/test_tb303_release_contract.cpp" "$ROOT/src/dsp/mini_tb303.cpp" "$ROOT/src/dsp/filter.cpp" "$ROOT/src/dsp/audio_wavetables.cpp" "$ROOT/src/dsp/tube_distortion.cpp" -o "$BUILD/release-$MODE"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/release-$MODE"
  "${CXX:-g++}" "${FLAGS[@]}" "${EXTRA[@]}" "$ROOT/tests/test_new_synth_voices.cpp" "${SOURCES[@]}" -o "$BUILD/voices-$MODE"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/voices-$MODE"
  "${CXX:-g++}" "${FLAGS[@]}" "${EXTRA[@]}" "$ROOT/tests/test_wave_morph_synth_voice.cpp" "$ROOT/src/dsp/wave_morph_synth_voice.cpp" -o "$BUILD/wavemorph-$MODE"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$BUILD/wavemorph-$MODE"
done
printf '%s\n' 'DSP-A focused host and ASan/UBSan: PASS'
