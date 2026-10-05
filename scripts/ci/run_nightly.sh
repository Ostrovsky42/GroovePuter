#!/usr/bin/env bash
set -euo pipefail

run_script() {
  local script="$1"
  echo "::group::${script}"
  bash "${script}"
  echo "::endgroup::"
}

bash scripts/ci/run_core.sh
bash scripts/ci/run_midi_targets.sh

for script in \
  tests/run_undo_0_9_8_r7_tests.sh \
  tests/run_0_9_9_undo_regression_tests.sh \
  tests/run_gf2_i1_tests.sh \
  tests/run_gf2_i2_tests.sh \
  tests/run_gf2_i2a_tests.sh \
  tests/run_gf2_i3_tests.sh \
  tests/run_gf2_i4_tests.sh \
  tests/run_gf2_i5_tests.sh \
  tests/run_gf2_c2_v0r_tests.sh \
  tests/run_gf2_gate_b_tests.sh \
  tests/run_gf2_semantic_orchestration_tests.sh \
  tests/run_gf2_c1df_final_distinctness_dependency.sh \
  tests/run_generation_stage15b_tests.sh \
  tests/run_generation_stage15c_tests.sh \
  tests/run_tonal_projector_tests.sh \
  tests/run_tonal_materializer_tests.sh \
  tests/run_tonal_materializer_global_scale_test.sh \
  tests/run_stage15_tonal_integration_tests.sh \
  tests/run_stage15_tonal_register_sweep.sh \
  tests/run_sampler_ref_tests.sh \
  tests/run_sampler_registry_boot_tests.sh \
  tests/run_sampler_persistence_ownership_tests.sh \
  tests/run_sampler_recovery_0_9_3_tests.sh \
  tests/run_tape_resource_recovery_tests.sh
do
  run_script "${script}"
done

run_script scripts/ci/run_stage15_baseline_contract.sh
