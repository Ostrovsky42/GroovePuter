#!/usr/bin/env bash
set -euo pipefail

run() {
  local name="$1"
  shift
  echo "::group::${name}"
  "$@"
  echo "::endgroup::"
}

run "CI surface" python3 scripts/ci/check_workflow_surface.py
run "Rhythm Stage 1" bash tests/run_rhythm_stage1_tests.sh
run "Rhythm Stage 2" bash tests/run_rhythm_stage2_tests.sh
run "Rhythm Stage 3" bash tests/run_rhythm_stage3_tests.sh
run "Rhythm Stage 4" bash tests/run_rhythm_stage4_tests.sh
run "Rhythm Stage 5" bash tests/run_rhythm_stage5_tests.sh
run "Rhythm Stage 6" bash tests/run_rhythm_stage6_tests.sh
run "Rhythm Stage 6.1" bash tests/run_rhythm_stage6_1_tests.sh
run "Phrase Core" bash scripts/ci/run_phrase_core.sh
run "Pattern/Phrase P1C" bash tests/run_pattern_phrase_p1c_tests.sh
run "Material identity" bash tests/run_0_9_11_c1a_material_identity_foundation.sh
run "Material UX" bash tests/run_0_9_11_c5_material_ux_tests.sh
run "CURRENT/NEXT causality" bash tests/run_fs2a_current_next_causality_tests.sh
run "Material discard" bash tests/run_material_discard_tests.sh
run "0.9.14 D0-C" bash tests/run_0_9_14_d0c_tests.sh
run "0.9.14 D0-D1" bash tests/run_0_9_14_d0d1_tests.sh
run "0.9.14 D0-E" bash tests/run_0_9_14_d0e_tests.sh
run "0.9.14 D0-F" bash tests/run_0_9_14_d0f_tests.sh
run "0.9.14 D1-A" bash tests/run_0_9_14_d1a_tests.sh
run "0.9.14 D1-B" bash tests/run_0_9_14_d1b_tests.sh
run "0.9.14 D1-B1" bash tests/run_0_9_14_d1b1_tests.sh
run "0.9.14 D1-C" bash tests/run_0_9_14_d1c_tests.sh
run "0.9.14 D1-C1" bash tests/run_0_9_14_d1c1_tests.sh
run "0.9.14 M0-A" bash tests/run_0_9_14_m0a_tests.sh
run "0.9.14 P0-B1" bash tests/run_0_9_14_p0b_tests.sh
run "0.9.14 P0 cycle" bash tests/run_0_9_14_p0_cycle_tests.sh
run "0.9.14 mix defaults" bash tests/run_0_9_14_mix_defaults_tests.sh
run "0.9.14 PML-C" bash tests/run_0_9_14_pml_c_tests.sh
run "0.9.14 PML-E" bash tests/run_0_9_14_pml_e_tests.sh
run "0.9.14 Save keeps cycle Undo" bash tests/run_0_9_14_save_keeps_undo_tests.sh
run "0.9.15 MIDI/PERFORM UI polish" bash tests/run_midi_perform_ui_polish_tests.sh
run "0.9.15 PERFORM scale arrows" bash tests/run_perform_page_arrows_tests.sh
run "0.9.15 PROJECT USB role row" bash tests/run_project_usb_role_ui_tests.sh
run "0.9.15 PROJECT LED brightness" bash tests/run_project_led_brightness_tests.sh
run "0.9.15 live note LED pulse" bash tests/run_live_note_led_tests.sh
run "0.9.15 external keyboard step entry" bash tests/run_external_step_entry_tests.sh
run "0.9.16 synth generation runtime" bash tests/run_synth_generation_runtime_tests.sh
run "Output ownership" bash tests/run_output_ownership_tests.sh
run "Performance closure" bash tests/run_performance_closure_tests.sh
run "Instrument interaction" bash tests/run_instrument_interaction_closure_tests.sh
run "Scale quantization" bash tests/run_scale_quantization_tests.sh
run "Pattern editor ownership" python3 tests/test_pattern_editor_input_ownership.py
run "Step note entry" python3 tests/test_step_note_entry_source_regressions.py
run "Host regressions" bash tests/run_host_tests.sh
