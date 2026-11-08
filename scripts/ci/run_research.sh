#!/usr/bin/env bash
set -euo pipefail

suite="${1:-}"
if [[ -z "${suite}" ]]; then
  echo "usage: $0 <gf2|phrase|generation|ui|undo|sampler|stage15|usb-acceptance|all>" >&2
  exit 2
fi

run_script() {
  local script="$1"
  test -f "${script}" || { echo "Missing preserved research asset: ${script}" >&2; exit 1; }
  echo "::group::${script}"
  bash "${script}"
  echo "::endgroup::"
}

run_suite() {
  case "$1" in
    gf2)
      for s in tests/run_gf2_i1_tests.sh tests/run_gf2_i2_tests.sh tests/run_gf2_i2a_tests.sh tests/run_gf2_i3_tests.sh tests/run_gf2_i4_tests.sh tests/run_gf2_i5_tests.sh tests/run_gf2_c2_v0r_tests.sh tests/run_gf2_gate_b_tests.sh tests/run_gf2_semantic_orchestration_tests.sh tests/run_gf2_c1df_final_distinctness_dependency.sh; do run_script "$s"; done
      ;;
    phrase)
      for s in tests/run_0_9_9_phrase_c1_tests.sh tests/run_0_9_9_phrase_h1_tests.sh tests/run_0_9_9_phrase_h1_f1_tests.sh tests/run_0_9_9_phrase_w1_tests.sh tests/run_0_9_9_phrase_w1r_tests.sh tests/run_0_9_9_phrase_h2r_tests.sh tests/run_0_9_9_phrase_p1r_tests.sh tests/run_0_9_9_phrase_i1_tests.sh tests/run_0_9_9_phrase_pmb_p1_tests.sh; do run_script "$s"; done
      ;;
    generation)
      for s in tests/run_generation_0_9_9_c_tests.sh tests/run_0_9_9_d1_tests.sh tests/run_0_9_9_d2_tests.sh tests/run_0_9_9_d3_tests.sh tests/run_0_9_9_e0a_tests.sh tests/run_0_9_9_e2a_tests.sh tests/run_0_9_9_e2b_tests.sh tests/run_0_9_9_e2c_tests.sh tests/run_0_9_9_e2t_tests.sh tests/run_0_9_9_m1_o1_tests.sh tests/run_0_9_9_m1_p1_tests.sh; do run_script "$s"; done
      ;;
    ui)
      for s in tests/run_0_9_9_ui_final_tests.sh tests/run_ui_constitution_u1a_tests.sh tests/run_ui_constitution_u1b_tests.sh tests/run_ui_constitution_u1c_tests.sh tests/run_ui_constitution_u1d_tests.sh tests/run_ui_constitution_u1e_tests.sh tests/run_ui_constitution_u1f_tests.sh tests/run_ui_constitution_u1g_tests.sh tests/run_ui_constitution_u2a_tests.sh tests/run_ui_constitution_u4a_tests.sh tests/run_ui_constitution_u4b1_tests.sh tests/run_ui_constitution_u4b2_tests.sh tests/run_ui_constitution_u4b3_tests.sh tests/run_ui_constitution_u4b4_tests.sh tests/run_ui_constitution_u4b5_tests.sh tests/run_pattern_phrase_p3_u1_tests.sh; do run_script "$s"; done
      ;;
    undo)
      for s in tests/run_undo_0_9_8_r1_tests.sh tests/run_undo_0_9_8_r2_tests.sh tests/run_undo_0_9_8_r3_tests.sh tests/run_undo_0_9_8_r4_tests.sh tests/run_undo_0_9_8_r5_tests.sh tests/run_undo_0_9_8_r6_tests.sh tests/run_undo_0_9_8_r7_tests.sh tests/run_0_9_9_undo_regression_tests.sh; do run_script "$s"; done
      ;;
    sampler)
      for s in tests/run_sampler_ref_tests.sh tests/run_sampler_registry_boot_tests.sh tests/run_sampler_persistence_ownership_tests.sh tests/run_sampler_recovery_0_9_3_tests.sh; do run_script "$s"; done
      ;;
    stage15)
      for s in tests/run_generation_stage15b_tests.sh tests/run_generation_stage15c_tests.sh tests/run_tonal_projector_tests.sh tests/run_tonal_materializer_tests.sh tests/run_tonal_materializer_global_scale_test.sh tests/run_stage15_tonal_integration_tests.sh tests/run_stage15_tonal_register_sweep.sh scripts/ci/run_stage15_baseline_contract.sh; do run_script "$s"; done
      ;;
    usb-acceptance)
      run_script scripts/ci/build_usb_acceptance_package.sh
      ;;
    all)
      for nested in gf2 phrase generation ui undo sampler stage15; do run_suite "$nested"; done
      ;;
    *)
      echo "Unknown research suite: $1" >&2
      exit 2
      ;;
  esac
}

run_suite "${suite}"
