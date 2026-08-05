#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_generation_rng_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_song_generation_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_synth_a_bass_profile_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_cardputer_input_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_scene_revision_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_ui_session_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_midi_file_manager_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_performance_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_wavemorph_performance_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_theme_selection_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_usb_midi_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_pattern_midi_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_midi_static_init_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_midi_transport_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_song_playhead_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_midi_companion_foundation_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_seqtrak_master_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_smf_midi_wave_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_atlas_sound_profile.py"
python3 "${ROOT_DIR}/tests/test_midi_probe.py"

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
  "${ROOT_DIR}/tests/test_performance_midi_targets.cpp" \
  "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
  -o "${BUILD_DIR}/test_performance_midi_targets"

"${BUILD_DIR}/test_performance_midi_targets"

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
  "${ROOT_DIR}/tests/test_scheduled_musical_event.cpp" \
  -o "${BUILD_DIR}/test_scheduled_musical_event"

"${BUILD_DIR}/test_scheduled_musical_event"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_scheduled_musical_event_queue.cpp" \
  -o "${BUILD_DIR}/test_scheduled_musical_event_queue"

"${BUILD_DIR}/test_scheduled_musical_event_queue"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_transport_sync.cpp" \
  -o "${BUILD_DIR}/test_midi_transport_sync"

"${BUILD_DIR}/test_midi_transport_sync"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_project_transport_timeline.cpp" \
  -o "${BUILD_DIR}/test_project_transport_timeline"

"${BUILD_DIR}/test_project_transport_timeline"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_project_transport_safety.cpp" \
  -o "${BUILD_DIR}/test_project_transport_safety"

"${BUILD_DIR}/test_project_transport_safety"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_document.cpp" \
  "${ROOT_DIR}/src/midi/smf_document.cpp" \
  -o "${BUILD_DIR}/test_smf_document"

"${BUILD_DIR}/test_smf_document"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_timing.cpp" \
  "${ROOT_DIR}/src/midi/smf_timing.cpp" \
  -o "${BUILD_DIR}/test_smf_timing"

"${BUILD_DIR}/test_smf_timing"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_scheduler.cpp" \
  "${ROOT_DIR}/src/midi/smf_scheduler.cpp" \
  "${ROOT_DIR}/src/midi/smf_timing.cpp" \
  -o "${BUILD_DIR}/test_smf_scheduler"

"${BUILD_DIR}/test_smf_scheduler"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_stream.cpp" \
  "${ROOT_DIR}/src/midi/smf_stream.cpp" \
  -o "${BUILD_DIR}/test_smf_stream"

"${BUILD_DIR}/test_smf_stream"



"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_channel_inspector.cpp" \
  -o "${BUILD_DIR}/test_smf_channel_inspector"

"${BUILD_DIR}/test_smf_channel_inspector"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_midi_visual.cpp" \
  -o "${BUILD_DIR}/test_smf_midi_visual"

"${BUILD_DIR}/test_smf_midi_visual"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_scheduled_smf_midi_event_queue.cpp" \
  -o "${BUILD_DIR}/test_scheduled_smf_midi_event_queue"

"${BUILD_DIR}/test_scheduled_smf_midi_event_queue"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_late_policy.cpp" \
  -o "${BUILD_DIR}/test_smf_late_policy"

"${BUILD_DIR}/test_smf_late_policy"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_dispatch_policy.cpp" \
  -o "${BUILD_DIR}/test_smf_dispatch_policy"

"${BUILD_DIR}/test_smf_dispatch_policy"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_routing.cpp" \
  -o "${BUILD_DIR}/test_smf_routing"

"${BUILD_DIR}/test_smf_routing"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_usb_midi_packet_pacer.cpp" \
  -o "${BUILD_DIR}/test_usb_midi_packet_pacer"

"${BUILD_DIR}/test_usb_midi_packet_pacer"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_control_event_queue.cpp" \
  -o "${BUILD_DIR}/test_midi_control_event_queue"

"${BUILD_DIR}/test_midi_control_event_queue"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_companion_settings.cpp" \
  "${ROOT_DIR}/src/midi/midi_companion_settings.cpp" \
  "${ROOT_DIR}/src/midi/midi_companion_settings_codec.cpp" \
  -o "${BUILD_DIR}/test_midi_companion_settings"

"${BUILD_DIR}/test_midi_companion_settings"

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
  "${ROOT_DIR}/tests/test_transport_clock_source.cpp" \
  -o "${BUILD_DIR}/test_transport_clock_source"

"${BUILD_DIR}/test_transport_clock_source"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_external_midi_transport_event_queue.cpp" \
  -o "${BUILD_DIR}/test_external_midi_transport_event_queue"

"${BUILD_DIR}/test_external_midi_transport_event_queue"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_external_midi_clock_tracker.cpp" \
  -o "${BUILD_DIR}/test_external_midi_clock_tracker"

"${BUILD_DIR}/test_external_midi_clock_tracker"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_usb_midi_realtime_parser.cpp" \
  -o "${BUILD_DIR}/test_usb_midi_realtime_parser"

"${BUILD_DIR}/test_usb_midi_realtime_parser"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_transport_clock_runtime.cpp" \
  -o "${BUILD_DIR}/test_transport_clock_runtime"

"${BUILD_DIR}/test_transport_clock_runtime"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_external_midi_clock_follower.cpp" \
  -o "${BUILD_DIR}/test_external_midi_clock_follower"

"${BUILD_DIR}/test_external_midi_clock_follower"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_usb_endpoint_health.cpp" \
  -o "${BUILD_DIR}/test_usb_endpoint_health"

"${BUILD_DIR}/test_usb_endpoint_health"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_external_transport_policy.cpp" \
  -o "${BUILD_DIR}/test_smf_external_transport_policy"

"${BUILD_DIR}/test_smf_external_transport_policy"


"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_scene_revision.cpp" \
  -o "${BUILD_DIR}/test_scene_revision"

"${BUILD_DIR}/test_scene_revision"


"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_file_name_policy.cpp" \
  -o "${BUILD_DIR}/test_midi_file_name_policy"

"${BUILD_DIR}/test_midi_file_name_policy"


"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_ui_session_state.cpp" \
  -o "${BUILD_DIR}/test_ui_session_state"

"${BUILD_DIR}/test_ui_session_state"


"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_cardputer_input_edges.cpp" \
  -o "${BUILD_DIR}/test_cardputer_input_edges"

"${BUILD_DIR}/test_cardputer_input_edges"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_wave_morph_synth_voice.cpp" \
  "${ROOT_DIR}/src/dsp/wave_morph_synth_voice.cpp" \
  -o "${BUILD_DIR}/test_wave_morph_synth_voice"

"${BUILD_DIR}/test_wave_morph_synth_voice"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_performance_patterns.cpp" \
  "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
  -o "${BUILD_DIR}/test_performance_patterns"

"${BUILD_DIR}/test_performance_patterns"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-unused-parameter \
  -Wno-misleading-indentation \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_tr606_drum_voice.cpp" \
  "${ROOT_DIR}/src/dsp/mini_drumvoices.cpp" \
  "${ROOT_DIR}/src/dsp/audio_wavetables.cpp" \
  "${ROOT_DIR}/src/dsp/tube_distortion.cpp" \
  -o "${BUILD_DIR}/test_tr606_drum_voice"

"${BUILD_DIR}/test_tr606_drum_voice"


"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_song_pattern_materializer.cpp" \
  -o "${BUILD_DIR}/test_song_pattern_materializer"

"${BUILD_DIR}/test_song_pattern_materializer"
