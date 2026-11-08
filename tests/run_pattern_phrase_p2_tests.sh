#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE_SHA="fa552763d34e0172ceed1d07913743165d9a5867"
# shellcheck source=tests/lib/candidate_base.sh
source "$ROOT/tests/lib/candidate_base.sh"
CANDIDATE_BASE_SHA="$(resolve_candidate_base "${P2_CANDIDATE_BASE_SHA:-}")"
TMP="${TMPDIR:-/tmp}/grooveputer_pattern_phrase_p2"
mkdir -p "$TMP"

git cat-file -e "${BASE_SHA}^{commit}"
if [[ "$(git merge-base HEAD "$BASE_SHA")" != "$BASE_SHA" ]]; then
  echo "P2 authoritative base ancestry verification failed" >&2
  exit 1
fi
printf '%s\n' 'P2 authoritative Gate-B base ancestry: PASS'

python3 tests/test_pattern_phrase_p2_source_contract.py
python3 tests/test_pattern_phrase_p2_executor_contract.py

# STOP/PAUSE ownership closure.
transport_status=0
bash tests/run_pattern_phrase_p2_transport_barrier.sh || transport_status=$?
owner_contract_status=0
python3 tests/test_pattern_phrase_p2_single_lifetime_owner_contract.py || owner_contract_status=$?
if (( transport_status != 0 || owner_contract_status != 0 )); then
  echo "P2 STOP/PAUSE ownership cutover is not GREEN" >&2
  exit 1
fi

# Task 7 remaining lifecycle/source barriers. Run both structural and observable
# evidence so a RED distinguishes architecture from backend behavior.
lifecycle_behavior_status=0
bash tests/run_pattern_phrase_p2_lifecycle_barriers.sh || lifecycle_behavior_status=$?
lifecycle_contract_status=0
python3 tests/test_pattern_phrase_p2_lifecycle_barrier_contract.py || lifecycle_contract_status=$?
if (( lifecycle_behavior_status != 0 || lifecycle_contract_status != 0 )); then
  echo "P2 lifecycle/source barrier closure is not GREEN" >&2
  exit 1
fi

CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror -I.)
COMMON_SRC=(src/phrase/runtime_synth_events.cpp)
PLAYBACK_SRC=(src/phrase/runtime_synth_playback.cpp)
PATTERN_BANK_SRC=()
if [[ -f src/phrase/runtime_pattern_event_bank.cpp ]]; then
  PATTERN_BANK_SRC+=(src/phrase/runtime_pattern_event_bank.cpp)
fi

build_suite() {
  local cxx="$1"
  local suffix="$2"
  shift 2
  local extra=("$@")

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PLAYBACK_SRC[@]}" \
    tests/test_pattern_phrase_p2_runtime_playback.cpp \
    -o "$TMP/p2-playback-$suffix"
  "$TMP/p2-playback-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" \
    tests/test_pattern_phrase_p2_conditional_expiry.cpp \
    -o "$TMP/p2-expiry-$suffix"
  "$TMP/p2-expiry-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_pattern_bank.cpp \
    -o "$TMP/p2-bank-$suffix"
  "$TMP/p2-bank-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_page_identity.cpp \
    -o "$TMP/p2-page-$suffix"
  "$TMP/p2-page-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_step_order.cpp \
    -o "$TMP/p2-step-order-$suffix"
  "$TMP/p2-step-order-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_rng_order.cpp \
    -o "$TMP/p2-rng-order-$suffix"
  "$TMP/p2-rng-order-$suffix"
}

build_suite g++ gcc | tee "$TMP/p2-gcc.out"
build_suite g++ gcc-repeat > "$TMP/p2-gcc-repeat.out"
diff -u "$TMP/p2-gcc.out" "$TMP/p2-gcc-repeat.out"
printf '%s\n' 'P2 deterministic GCC repeat: PASS'

if command -v clang++ >/dev/null 2>&1; then
  build_suite clang++ clang > "$TMP/p2-clang.out"
  diff -u "$TMP/p2-gcc.out" "$TMP/p2-clang.out"
  printf '%s\n' 'P2 Clang parity: PASS'
fi

ASAN_OPTIONS=detect_leaks=0 build_suite g++ asan \
  -fsanitize=address -fno-omit-frame-pointer > /dev/null
printf '%s\n' 'P2 ASan: PASS'

build_suite g++ ubsan \
  -fsanitize=undefined -fno-sanitize-recover=undefined > /dev/null
printf '%s\n' 'P2 UBSan: PASS'

# Scope the whitespace gate to the candidate. Diffing from the historical
# BASE_SHA flagged whitespace in files the candidate never touched.
git cat-file -e "${CANDIDATE_BASE_SHA}^{commit}"
git diff --check "${CANDIDATE_BASE_SHA}"..HEAD
git diff --check
printf '%s\n' 'PATTERN/PHRASE P2 focused gate: PASS'
