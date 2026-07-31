#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_performance_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_usb_midi_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_pattern_midi_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_song_playhead_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_atlas_sound_profile.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_genre_defaults.cpp" \
  -o "${BUILD_DIR}/test_genre_defaults"

"${BUILD_DIR}/test_genre_defaults"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_atlas_runtime.cpp" \
  "${ROOT_DIR}/src/dsp/atlas_runtime.cpp" \
  -o "${BUILD_DIR}/test_atlas_runtime"

"${BUILD_DIR}/test_atlas_runtime"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_performance_keyboard.cpp" \
  "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
  -o "${BUILD_DIR}/test_performance_keyboard"

"${BUILD_DIR}/test_performance_keyboard"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_song_cycle_boundary.cpp" \
  -o "${BUILD_DIR}/test_song_cycle_boundary"

"${BUILD_DIR}/test_song_cycle_boundary"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_usb_midi_output.cpp" \
  "${ROOT_DIR}/src/midi/usb_midi_output.cpp" \
  -o "${BUILD_DIR}/test_usb_midi_output"

"${BUILD_DIR}/test_usb_midi_output"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_musical_event_queue.cpp" \
  -o "${BUILD_DIR}/test_musical_event_queue"

"${BUILD_DIR}/test_musical_event_queue"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_voice.cpp" \
  "${ROOT_DIR}/src/sampler/sampler_voice.cpp" \
  -o "${BUILD_DIR}/test_sampler_voice"

"${BUILD_DIR}/test_sampler_voice"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -pthread \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_audio_mutation_gate.cpp" \
  -o "${BUILD_DIR}/test_audio_mutation_gate"

"${BUILD_DIR}/test_audio_mutation_gate"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"${ROOT_DIR}" \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  "${ROOT_DIR}/tests/test_scene_roundtrip.cpp" \
  "${ROOT_DIR}/scenes.cpp" \
  "${ROOT_DIR}/json_evented.cpp" \
  "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
  -o "${BUILD_DIR}/test_scene_roundtrip"

"${BUILD_DIR}/test_scene_roundtrip"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"${ROOT_DIR}" \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  "${ROOT_DIR}/tests/test_pattern_paging.cpp" \
  "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
  -o "${BUILD_DIR}/test_pattern_paging"

"${BUILD_DIR}/test_pattern_paging"
echo "host regressions: OK"
