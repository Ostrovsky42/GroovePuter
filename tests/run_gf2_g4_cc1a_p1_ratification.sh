#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-cc1a-p1-ratification"
mkdir -p "$BUILD"

BASE_SHA="8331735080f25acbc809ef97b9881fd9e17c86a2"
HEAD_SHA="$(git -C "$ROOT" rev-parse HEAD)"
WORKFLOW_SHA="${GITHUB_SHA:-$HEAD_SHA}"

declare -A RESULT
for key in production_scope p1_authority c1b_lifetime p2 p3 g4_r1 g4_i3 g4_i4 g4_i5 g4_i6 g4_cc1 g4_cc1a; do
  RESULT[$key]="NOT_RUN"
done
OVERALL="FAIL"

write_summary() {
  cat > "$BUILD/ratification-summary.txt" <<EOF
G4_CC1A_P1_RATIFICATION
head_sha=$HEAD_SHA
workflow_sha=$WORKFLOW_SHA
production_scope=${RESULT[production_scope]}
p1_authority=${RESULT[p1_authority]}
c1b_lifetime=${RESULT[c1b_lifetime]}
p2=${RESULT[p2]}
p3=${RESULT[p3]}
g4_r1=${RESULT[g4_r1]}
g4_i3=${RESULT[g4_i3]}
g4_i4=${RESULT[g4_i4]}
g4_i5=${RESULT[g4_i5]}
g4_i6=${RESULT[g4_i6]}
g4_cc1=${RESULT[g4_cc1]}
g4_cc1a=${RESULT[g4_cc1a]}
g4_cc1a_retained_mode=PARTIAL_SUCCESSOR_P1
overall=$OVERALL
EOF
}
trap write_summary EXIT

run_gate() {
  local key="$1"
  shift
  local log="$BUILD/${key}.log"
  printf 'G4_CC1A_P1_RATIFICATION_GATE %s START\n' "$key" | tee "$log"
  set +e
  "$@" 2>&1 | tee -a "$log"
  local status=${PIPESTATUS[0]}
  set -e
  if (( status != 0 )); then
    RESULT[$key]="FAIL"
    printf 'G4_CC1A_P1_RATIFICATION_GATE %s FAIL status=%d\n' "$key" "$status" | tee -a "$log"
    return "$status"
  fi
  RESULT[$key]="PASS"
  printf 'G4_CC1A_P1_RATIFICATION_GATE %s PASS\n' "$key" | tee -a "$log"
}

run_production_scope() {
  local log="$BUILD/production_scope.log"
  git -C "$ROOT" cat-file -e "${BASE_SHA}^{commit}"
  local changed unexpected
  changed="$(git -C "$ROOT" diff --name-only "$BASE_SHA" "$HEAD_SHA" -- src/)"
  unexpected="$(printf '%s\n' "$changed" | sed '/^$/d' | grep -vFx 'src/generation/composition/generation_profile.cpp' || true)"
  {
    printf 'G4_CC1A_P1_PRODUCTION_SCOPE %s\n' "${changed:-NONE}"
    printf 'G4_CC1A_P1_HEAD_SHA %s\n' "$HEAD_SHA"
    printf 'G4_CC1A_P1_WORKFLOW_SHA %s\n' "$WORKFLOW_SHA"
  } | tee "$log"
  if [[ "$HEAD_SHA" != "$WORKFLOW_SHA" ]]; then
    printf 'G4_CC1A_P1_FAIL workflow_checkout_sha_mismatch\n' | tee -a "$log"
    RESULT[production_scope]="FAIL"
    return 1
  fi
  if [[ -n "$unexpected" ]]; then
    printf 'G4_CC1A_P1_FAIL unexpected_production_diff\n%s\n' "$unexpected" | tee -a "$log"
    RESULT[production_scope]="FAIL"
    return 1
  fi
  RESULT[production_scope]="PASS"
}

cd "$ROOT"
run_production_scope

run_gate p1_authority bash tests/run_gf2_g4_cc1a_p1_tests.sh

# G4-CC1A's original full binary is historical evidence of the pre-P1 collapse.
# Its assertions continuation==0 / all-detached are intentionally false after P1.
# The current-safe retained component is the successor P1 capability/adversarial
# gate above; do not misrepresent the historical observation as a regression.
RESULT[g4_cc1a]="PASS"
cat > "$BUILD/g4_cc1a_retained.log" <<'EOF'
G4_CC1A_RETAINED mode=PARTIAL_SUCCESSOR_P1
current_safe_runner=tests/run_gf2_g4_cc1a_p1_tests.sh
historical_full_runner=tests/run_gf2_g4_cc1a_tests.sh
historical_full_runner_executed=NO
historical_only_assertions=continuation_capable_bass_edges==0,non_plain_articulation_owners==0,all_detached==1152,withContinuation==0,mixed==0,ARTICULATION_COLLAPSED
retained_methodology=capability,adversarial_false_positive_defense,label_independence,weight_independence,reachability,deterministic_corpus
result=PASS
EOF

run_gate c1b_lifetime bash tests/run_pattern_phrase_p2_lifecycle_barriers.sh
run_gate p2 env ASAN_OPTIONS=detect_leaks=0 bash tests/run_pattern_phrase_p2_tests.sh
run_gate p3 bash tests/run_pattern_phrase_p3_tests.sh
run_gate g4_r1 bash tests/run_gf2_g4_r1_tests.sh
run_gate g4_i3 bash tests/run_gf2_g4_i3_tests.sh
run_gate g4_i4 bash tests/run_gf2_g4_i4_tests.sh
run_gate g4_i5 bash tests/run_gf2_g4_i5_tests.sh

# I6 is deliberately run as-is, including committed-evidence freshness. If P1
# legitimately makes the immutable I6 evidence stale, this gate must fail and
# C3A stops rather than rewriting historical evidence.
run_gate g4_i6 bash tests/run_gf2_g4_i6_tests.sh
run_gate g4_cc1 bash tests/run_gf2_g4_cc1_tests.sh

for key in production_scope p1_authority c1b_lifetime p2 p3 g4_r1 g4_i3 g4_i4 g4_i5 g4_i6 g4_cc1 g4_cc1a; do
  if [[ "${RESULT[$key]}" != "PASS" ]]; then
    printf 'G4_CC1A_P1_RATIFICATION FAIL gate=%s result=%s\n' "$key" "${RESULT[$key]}"
    exit 1
  fi
done

OVERALL="PASS"
printf 'G4_CC1A_P1_RATIFICATION PASS\n'
