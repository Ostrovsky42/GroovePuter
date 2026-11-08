#!/usr/bin/env bash
set -euo pipefail

run() {
  local name="$1"
  shift
  echo "::group::${name}"
  "$@"
  echo "::endgroup::"
}

run "MIDI profile model" bash tests/run_midi_device_profiles_0_9_7_tests.sh
run "MIDI capabilities" bash tests/run_midi_device_capabilities_0_9_7_tests.sh
run "MIDI profile runtime" bash tests/run_midi_device_profile_runtime_0_9_7_tests.sh
run "MIDI output route" bash tests/run_midi_output_route_projection_0_9_7_tests.sh
run "MIDI pattern binding" bash tests/run_midi_pattern_route_binding_0_9_7_tests.sh
run "MIDI settings boot order" bash tests/run_midi_settings_boot_order_0_9_7_tests.sh
run "MIDI performance binding" bash tests/run_midi_performance_route_binding_0_9_7_tests.sh
run "MIDI profile selection" bash tests/run_midi_profile_selection_0_9_7_tests.sh
run "MIDI profile UI" bash tests/run_midi_device_profile_ui_0_9_7_tests.sh
run "0.9.11 MIDI convergence" bash tests/run_0_9_11_c4_midi_convergence_tests.sh
run "C9 MIDI repair" bash tests/run_c9_midi_repair_tests.sh
