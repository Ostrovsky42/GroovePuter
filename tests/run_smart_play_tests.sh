#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/smart-play-tests"
CXX="${CXX:-g++}"
EXTRA_CXXFLAGS="${SMART_PLAY_CXXFLAGS:-}"
mkdir -p "${BUILD_DIR}"

usage() {
  echo "usage: $0 [model] [quality] [snapshot] [registry] [legacy_context] [bridge] [config] [ownership] [dispatch] [midi_input] [midi_queue] [external_queue] [arp] [trace] [keyboard] [external] [cr_capture] [cr_motif] [cr_tap] [cr_planner] [cr_cancel] [cr_timing] [cr_lookahead] [cr_genre] [cr_single_note] [cr_channel] [cr_settings] [cr_development] [cr_four_bars] [cr_loop_grid] [cr_phase6] [cr_loop_gaps] [cr_phase8] [cr_phase9] [cr_phase10] [pattern_range_wire] [pattern_range_engine]" >&2
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

run_config() {
  compile_run smart_play_config \
    "${ROOT_DIR}/tests/test_smart_play_config.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard_config.cpp" \
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

# CALL/RESPONSE phase 0: capture + descriptor + PERFORM input tap (no sound).
run_cr_capture() {
  compile_run cr_phrase_capture "${ROOT_DIR}/tests/test_cr_phrase_capture.cpp"
}

run_cr_motif() {
  compile_run cr_motif_descriptor "${ROOT_DIR}/tests/test_cr_motif_descriptor.cpp"
}

run_cr_tap() {
  compile_run cr_keyboard_tap \
    "${ROOT_DIR}/tests/test_cr_keyboard_tap.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp"
}

# CALL/RESPONSE phase 1: template answer (planner, cancel/turn-taking, timing).
run_cr_planner() {
  compile_run cr_response_planner "${ROOT_DIR}/tests/test_cr_response_planner.cpp"
}

run_cr_cancel() {
  compile_run cr_response_cancel \
    "${ROOT_DIR}/tests/test_cr_response_cancel.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp"
}

run_cr_timing() {
  compile_run cr_answer_timing "${ROOT_DIR}/tests/test_cr_answer_timing.cpp"
}

# CALL/RESPONSE phase 2: next-bar harmony lookahead + genre weights.
run_cr_lookahead() {
  compile_run cr_harmony_lookahead \
    "${ROOT_DIR}/tests/test_cr_harmony_lookahead.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

run_cr_genre() {
  compile_run cr_genre_weights "${ROOT_DIR}/tests/test_cr_genre_weights.cpp"
}

# CALL/RESPONSE phase 3: one-note answer fix, answer channel lane, PARTNER settings.
run_cr_single_note() {
  compile_run cr_single_note "${ROOT_DIR}/tests/test_cr_single_note.cpp"
}

run_cr_channel() {
  compile_run cr_answer_channel \
    "${ROOT_DIR}/tests/test_cr_answer_channel.cpp" \
    "${ROOT_DIR}/src/midi/usb_midi_output.cpp"
}

run_cr_development() {
  compile_run cr_development "${ROOT_DIR}/tests/test_cr_development.cpp"
}

run_cr_settings() {
  compile_run cr_partner_settings "${ROOT_DIR}/tests/test_cr_partner_settings.cpp"
}

# CALL/RESPONSE phase 4: PARTNER BARS = 4 (capture, 64-bit mask, planner, setting, lookahead).
run_cr_loop_grid() {
  compile_run cr_loop_grid "${ROOT_DIR}/tests/test_cr_loop_grid.cpp"
}

run_cr_four_bars() {
  compile_run cr_four_bars \
    "${ROOT_DIR}/tests/test_cr_four_bars.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

# CALL/RESPONSE phase 6: per-note answer velocity, chord/form-aware ending.
run_cr_phase6() {
  compile_run cr_phase6_music "${ROOT_DIR}/tests/test_cr_phase6_music.cpp"
}

run_cr_loop_gaps() {
  compile_run cr_loop_gaps "${ROOT_DIR}/tests/test_cr_loop_gaps.cpp"
}

# CALL/RESPONSE phase 8: tonic from the snapshot, template chords, genre ending
# colour, LOOP pass form.
run_cr_phase8() {
  compile_run cr_phase8_variety \
    "${ROOT_DIR}/tests/test_cr_phase8_variety.cpp" \
    "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp"
}

# CALL/RESPONSE phase 9: PARTNER LENGTH (DELAY removed, answers on the grid).
run_cr_phase9() {
  compile_run cr_phase9_length "${ROOT_DIR}/tests/test_cr_phase9_length.cpp"
}

# CALL/RESPONSE phase 10: PERFORM > INPUT hides PARTNER GENRE and LENGTH.
# The page needs the SDL host source set (same build as
# run_perform_page_arrows_tests.sh); no -Werror over the whole UI tree.
run_cr_phase10() {
  local build="${BUILD_DIR}/cr_phase10"
  mkdir -p "${build}"
  pushd "${ROOT_DIR}/platform_sdl" >/dev/null
  local sources
  mapfile -t sources < <(
    awk '/^SOURCES :=/ { capture = 1; next }
         capture && /^[^[:space:]]/ { capture = 0 }
         capture { line = $0; sub(/^[[:space:]]+/, "", line);
                   sub(/[[:space:]]*\\[[:space:]]*$/, "", line);
                   n = split(line, parts, /[[:space:]]+/);
                   for (i = 1; i <= n; i++)
                     if (parts[i] != "" && parts[i] != "sdl_main.cpp") print parts[i] }' Makefile
  )
  if [[ "${#sources[@]}" -eq 0 ]]; then
    echo 'failed to resolve SDL source set' >&2
    exit 2
  fi
  # shellcheck disable=SC2046
  "${CXX}" -std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h \
    $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
    "${sources[@]}" ../tests/test_cr_phase10_menu.cpp \
    $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "${build}/test"
  popd >/dev/null
  "${build}/test"
}

# PATTERN_RANGE_MIDI: steps hold C1..C7; MIDI gets the note as written.
# Wire + editor clamp: pure, -Werror. Old 0.9.8-R3 mutation contract and the
# NOTE ENTRY source contract follow the new ceiling.
run_pattern_range_wire() {
  compile_run pattern_range_wire \
    "${ROOT_DIR}/tests/test_pattern_range_wire.cpp" \
    "${ROOT_DIR}/src/midi/usb_midi_output.cpp" \
    "${ROOT_DIR}/src/midi/midi_companion_settings.cpp"
  compile_run pattern_mutations_0_9_8_r3 \
    "${ROOT_DIR}/tests/test_pattern_mutations_0_9_8_r3.cpp"
  python3 "${ROOT_DIR}/tests/test_step_note_entry_source_regressions.py"
}

# PATTERN_RANGE_MIDI on the real engine and STEPS page: NOTE ENTRY E5/C#5,
# S/X to C7, Pattern NoteOn = step note, internal voice folded, save/load.
# SDL host source set, as run_cr_phase10.
run_pattern_range_engine() {
  local build="${BUILD_DIR}/pattern_range_engine"
  mkdir -p "${build}"
  pushd "${ROOT_DIR}/platform_sdl" >/dev/null
  local sources
  mapfile -t sources < <(
    awk '/^SOURCES :=/ { capture = 1; next }
         capture && /^[^[:space:]]/ { capture = 0 }
         capture { line = $0; sub(/^[[:space:]]+/, "", line);
                   sub(/[[:space:]]*\\[[:space:]]*$/, "", line);
                   n = split(line, parts, /[[:space:]]+/);
                   for (i = 1; i <= n; i++)
                     if (parts[i] != "" && parts[i] != "sdl_main.cpp") print parts[i] }' Makefile
  )
  if [[ "${#sources[@]}" -eq 0 ]]; then
    echo 'failed to resolve SDL source set' >&2
    exit 2
  fi
  # shellcheck disable=SC2046
  "${CXX}" -std=c++17 -Wno-c++20-extensions -I.. -I. -include arduino_compat.h \
    $(sdl2-config --cflags) $(pkg-config --cflags SDL2_gfx) -O1 \
    "${sources[@]}" ../tests/test_pattern_range_engine.cpp \
    $(sdl2-config --libs) $(pkg-config --libs SDL2_gfx) -o "${build}/test"
  popd >/dev/null
  "${build}/test"
}

targets=("$@")
if ((${#targets[@]} == 0)); then
  targets=(model quality snapshot registry legacy_context bridge config ownership dispatch midi_input midi_queue external_queue arp trace keyboard external cr_capture cr_motif cr_tap cr_planner cr_cancel cr_timing cr_lookahead cr_genre cr_single_note cr_channel cr_settings cr_development cr_four_bars cr_loop_grid cr_phase6 cr_loop_gaps cr_phase8 cr_phase9 cr_phase10 pattern_range_wire pattern_range_engine)
fi

for target in "${targets[@]}"; do
  case "${target}" in
    model) run_model ;;
    quality) run_quality ;;
    snapshot) run_snapshot ;;
    registry) run_registry ;;
    legacy_context) run_legacy_context ;;
    bridge) run_bridge ;;
    config) run_config ;;
    ownership) run_ownership ;;
    dispatch) run_dispatch ;;
    trace) run_trace ;;
    midi_input) run_midi_input ;;
    midi_queue) run_midi_queue ;;
    external_queue) run_external_queue ;;
    arp) run_arp ;;
    keyboard) run_keyboard ;;
    external) run_external ;;
    cr_capture) run_cr_capture ;;
    cr_motif) run_cr_motif ;;
    cr_tap) run_cr_tap ;;
    cr_planner) run_cr_planner ;;
    cr_cancel) run_cr_cancel ;;
    cr_timing) run_cr_timing ;;
    cr_lookahead) run_cr_lookahead ;;
    cr_genre) run_cr_genre ;;
    cr_single_note) run_cr_single_note ;;
    cr_channel) run_cr_channel ;;
    cr_settings) run_cr_settings ;;
    cr_development) run_cr_development ;;
    cr_four_bars) run_cr_four_bars ;;
    cr_loop_grid) run_cr_loop_grid ;;
    cr_phase6) run_cr_phase6 ;;
    cr_loop_gaps) run_cr_loop_gaps ;;
    cr_phase8) run_cr_phase8 ;;
    cr_phase9) run_cr_phase9 ;;
    cr_phase10) run_cr_phase10 ;;
    pattern_range_wire) run_pattern_range_wire ;;
    pattern_range_engine) run_pattern_range_engine ;;
    *) usage; echo "unknown target: ${target}" >&2; exit 2 ;;
  esac
done

echo "smart-play tests passed: ${targets[*]}"
