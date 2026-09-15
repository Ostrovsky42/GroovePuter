#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "======================================================================"
echo "      GroovePuter 0.9.12 Full Release Regression Suite                "
echo "======================================================================"

run_suite() {
  local name="$1"
  local cmd="$2"
  echo ">>> RUNNING: ${name} ..."
  eval "${cmd}"
  echo ">>> [OK] ${name}"
  echo ""
}

# 1. Identity & Persistence
run_suite "C1A MaterialId Persistence" "bash tests/run_0_9_11_c1a_material_id_persistence.sh"
run_suite "C1A Legacy MaterialId Compatibility" "bash tests/run_0_9_11_c1a_material_id_legacy.sh"
run_suite "Paging & Ownership Persistence" "bash tests/run_0_9_9_d1_tests.sh"

# 2. Melody Storage & Promotion
run_suite "Melody Store Format" "bash tests/run_melody_store_tests.sh"
run_suite "Melody Promotion" "bash tests/run_melody_promotion_tests.sh"
run_suite "Melody Audible Fail-Closed" "bash tests/run_c9a_melody_audible_fail_closed.sh"

# 3. Material Working & Identity Bounds
run_suite "M-WORKING A2-B Integration" "bash tests/run_0_9_11_m_working_a2b_integration.sh"
run_suite "M-WORKING A2-B MWK" "bash tests/run_0_9_11_m_working_a2b_mwk_tests.sh"
run_suite "M-WORKING Manual Pattern Edit" "bash tests/run_0_9_11_m_working_manual_pattern_edit_tests.sh"
run_suite "M-WORKING Manual Retarget" "bash tests/run_0_9_11_m_working_manual_retarget_tests.sh"

# 4. Lifecycle & Causality Gates
run_suite "FS2A Current/Next Causality" "bash tests/run_fs2a_current_next_causality_tests.sh"
run_suite "M4 Next Material" "bash tests/run_m4_next_material_tests.sh"
run_suite "Material DISCARD" "bash tests/run_material_discard_tests.sh"
run_suite "Material LENGTH" "bash tests/run_material_length_tests.sh"

# 5. Material ACCEPT & Cold-Boot Recovery Suite
run_suite "Material ACCEPT & Durability" "bash tests/run_material_accept_tests.sh"

echo "======================================================================"
echo "   ALL 0.9.12 FULL RELEASE REGRESSION SUITES PASSED (15/15) GREEN!   "
echo "======================================================================"
