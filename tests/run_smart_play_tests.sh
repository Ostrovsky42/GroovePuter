#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/smart-play-tests"
CXX="${CXX:-g++}"
EXTRA_CXXFLAGS="${SMART_PLAY_CXXFLAGS:-}"
mkdir -p "${BUILD_DIR}"

usage() {
  echo "usage: $0 [model] [quality] [snapshot] [registry] [legacy_context] [bridge] [ownership] [dispatch] [midi_input] [midi_queue] [external_queue] [arp] [trace] [keyboard] [external]" >&2
}

compile_run() {
  local name="$1"
  shift
  # Intentional word splitting permits caller-provided sanitizer/compiler flags.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions ${EXTRA_CXXFLAGS} \
    -I"${ROOT_DIR}" "$@" -o "${BUILD_DIR}/test_${name}"
  "${BUILD_DIR}/test_${name}"
}

run_model() {
  compile_run performance_smart_play "${ROOT_DIR}/tests/test_performance_smart_play.cpp"
}

run_quality() {
  compile_run performance_chord_quality "${ROOT_DIR}/tests/test_performance_chord_quality.cpp"
}

run_snapshot() {
  compile_run harmonic_snapshot "${ROOT_DIR}/tests/test_harmonic_snapshot.cpp"
}

run_registry() {
  python3 "${ROOT_DIR}/tests/test_harmonic_registry_generation.py"
}

run_legacy_context() {
  compile_run performance_harmony_context \
    "${ROOT_DIR}/tests/test_performance_harmony_context.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

run_bridge() {
  compile_run smart_play_harmony_bridge \
    "${ROOT_DIR}/tests/test_smart_play_harmony_bridge.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

run_ownership() {
  compile_run smart_play_input_ownership \
    "${ROOT_DIR}/tests/test_smart_play_input_ownership.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp"
}

run_dispatch() {
  compile_run smart_play_dispatch \
    "${ROOT_DIR}/tests/test_smart_play_dispatch.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

run_trace() {
  compile_run smart_arp_event_trace \
    "${ROOT_DIR}/tests/test_smart_arp_event_trace.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

run_midi_input() {
  compile_run c9_midi_input_dispatcher \
    "${ROOT_DIR}/tests/test_c9_midi_input_dispatcher.cpp"
}

run_midi_queue() {
  compile_run midi_input_queue "${ROOT_DIR}/tests/test_0_9_11_c4_midi_ingress.cpp"
}

run_external_queue() {
  compile_run external_note_queue "${ROOT_DIR}/tests/test_external_note_queue.cpp"
}

run_arp() {
  compile_run performance_smart_arp \
    "${ROOT_DIR}/tests/test_performance_smart_arp.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp"
}

run_keyboard() {
  compile_run performance_keyboard \
    "${ROOT_DIR}/tests/test_performance_keyboard.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp"
}

run_external() {
  compile_run performance_keyboard_external \
    "${ROOT_DIR}/tests/test_performance_keyboard_external.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp"
}

targets=("$@")
if ((${#targets[@]} == 0)); then
  targets=(model quality snapshot registry legacy_context bridge ownership dispatch midi_input midi_queue external_queue arp trace keyboard external)
fi

for target in "${targets[@]}"; do
  case "${target}" in
    model) run_model ;;
    quality) run_quality ;;
    snapshot) run_snapshot ;;
    registry) run_registry ;;
    legacy_context) run_legacy_context ;;
    bridge) run_bridge ;;
    ownership) run_ownership ;;
    dispatch) run_dispatch ;;
    trace) run_trace ;;
    midi_input) run_midi_input ;;
    midi_queue) run_midi_queue ;;
    external_queue) run_external_queue ;;
    arp) run_arp ;;
    keyboard) run_keyboard ;;
    external) run_external ;;
    *) usage; echo "unknown target: ${target}" >&2; exit 2 ;;
  esac
done

echo "smart-play tests passed: ${targets[*]}"
