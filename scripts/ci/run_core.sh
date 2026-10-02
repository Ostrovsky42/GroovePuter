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
run "Pattern/Phrase P2" bash tests/run_pattern_phrase_p2_tests.sh
run "Pattern/Phrase P3" bash tests/run_pattern_phrase_p3_tests.sh
run "Pattern/Phrase P3-U1" bash tests/run_pattern_phrase_p3_u1_tests.sh
run "Material identity" bash tests/run_0_9_11_c1a_material_identity_foundation.sh
run "Material UX" bash tests/run_0_9_11_c5_material_ux_tests.sh
run "CURRENT/NEXT causality" bash tests/run_fs2a_current_next_causality_tests.sh
run "Material discard" bash tests/run_material_discard_tests.sh
run "0.9.14 D0-C" bash tests/run_0_9_14_d0c_tests.sh
run "0.9.14 D0-D1" bash tests/run_0_9_14_d0d1_tests.sh
run "0.9.14 D0-E" bash tests/run_0_9_14_d0e_tests.sh
run "0.9.14 D0-F" bash tests/run_0_9_14_d0f_tests.sh
run "Output ownership" bash tests/run_output_ownership_tests.sh
run "Performance closure" bash tests/run_performance_closure_tests.sh
run "Instrument interaction" bash tests/run_instrument_interaction_closure_tests.sh
run "Scale quantization" bash tests/run_scale_quantization_tests.sh
run "Pattern editor ownership" python3 tests/test_pattern_editor_input_ownership.py
run "Step note entry" python3 tests/test_step_note_entry_source_regressions.py
run "Host regressions" bash tests/run_host_tests.sh
