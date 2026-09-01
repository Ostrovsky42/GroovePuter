#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPORT_DIR="${GF2_VALIDATION_REPORT_DIR:-${ROOT}/build/gf2-validation}"
mkdir -p "${REPORT_DIR}"

GATES=(HOST SDL CARDPUTER_ADV FIXED_DRAM SEQTRAK_MIDI_ONLY)
declare -A STATUS
declare -A DETAIL
declare -A REQUESTED

for gate in "${GATES[@]}"; do
  STATUS["${gate}"]="NOT RUN"
  DETAIL["${gate}"]="not requested"
  REQUESTED["${gate}"]=0
done

usage() {
  cat <<'EOF'
Usage: bash scripts/validate_gf2_targets.sh [--all|host|sdl|cardputer|dram|seqtrak]...

Runs existing authoritative GroovePuter validation commands and writes:
  build/gf2-validation/matrix.txt
  build/gf2-validation/matrix.json

Exit codes:
  0  every requested gate PASS
  1  at least one requested gate FAIL
  2  no requested gate FAIL, but at least one requested gate UNAVAILABLE
EOF
}

request_gate() {
  case "$1" in
    host) REQUESTED[HOST]=1 ;;
    sdl) REQUESTED[SDL]=1 ;;
    cardputer) REQUESTED[CARDPUTER_ADV]=1 ;;
    dram) REQUESTED[FIXED_DRAM]=1 ;;
    seqtrak) REQUESTED[SEQTRAK_MIDI_ONLY]=1 ;;
    *)
      echo "Unknown GF2 validation gate: $1" >&2
      usage >&2
      exit 64
      ;;
  esac
}

if (( $# == 0 )); then
  set -- --all
fi

for arg in "$@"; do
  case "${arg}" in
    --all)
      for gate in "${GATES[@]}"; do REQUESTED["${gate}"]=1; done
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    host|sdl|cardputer|dram|seqtrak)
      request_gate "${arg}"
      ;;
    *)
      echo "Unknown argument: ${arg}" >&2
      usage >&2
      exit 64
      ;;
  esac
done

have_command() {
  command -v "$1" >/dev/null 2>&1
}

mark_unavailable() {
  local gate="$1"
  local reason="$2"
  STATUS["${gate}"]="UNAVAILABLE"
  DETAIL["${gate}"]="${reason}"
}

run_gate_command() {
  local gate="$1"
  local detail="$2"
  shift 2

  echo "=== GF2 ${gate} ==="
  set +e
  "$@"
  local rc=$?
  set -e

  if (( rc == 0 )); then
    STATUS["${gate}"]="PASS"
    DETAIL["${gate}"]="${detail}"
  else
    STATUS["${gate}"]="FAIL"
    DETAIL["${gate}"]="${detail}; exit=${rc}"
  fi
}

run_host() {
  if [[ "${REQUESTED[HOST]}" != 1 ]]; then return; fi
  for tool in bash g++ python3; do
    if ! have_command "${tool}"; then
      mark_unavailable HOST "missing prerequisite: ${tool}"
      return
    fi
  done

  echo "=== GF2 HOST ==="
  set +e
  (
    cd "${ROOT}"
    bash tests/run_host_tests.sh &&
      bash tests/run_generation_0_9_9_c_tests.sh &&
      bash tests/run_gf2_c2_v0r_tests.sh
  )
  local rc=$?
  set -e
  if (( rc == 0 )); then
    STATUS[HOST]="PASS"
    DETAIL[HOST]="run_host_tests + 0.9.9-C/I0R + C2-V0R"
  else
    STATUS[HOST]="FAIL"
    DETAIL[HOST]="authoritative host regression chain; exit=${rc}"
  fi
}

run_sdl() {
  if [[ "${REQUESTED[SDL]}" != 1 ]]; then return; fi
  if [[ -n "${GF2_SDL_SETUP_STATUS:-}" && "${GF2_SDL_SETUP_STATUS}" != "success" ]]; then
    mark_unavailable SDL "remote SDL dependency setup=${GF2_SDL_SETUP_STATUS}"
    return
  fi
  for tool in make g++; do
    if ! have_command "${tool}"; then
      mark_unavailable SDL "missing prerequisite: ${tool}"
      return
    fi
  done
  if ! have_command sdl2-config; then
    mark_unavailable SDL "missing prerequisite: sdl2-config"
    return
  fi
  if ! have_command pkg-config || ! pkg-config --exists SDL2_gfx; then
    mark_unavailable SDL "missing prerequisite: SDL2_gfx pkg-config metadata"
    return
  fi

  run_gate_command SDL "make -C platform_sdl clean all CXX=g++" \
    make -C "${ROOT}/platform_sdl" clean all CXX=g++
}

arduino_prerequisites_available() {
  local gate="$1"
  if [[ -n "${GF2_ARDUINO_CLI_SETUP_STATUS:-}" && "${GF2_ARDUINO_CLI_SETUP_STATUS}" != "success" ]]; then
    mark_unavailable "${gate}" "remote arduino-cli setup=${GF2_ARDUINO_CLI_SETUP_STATUS}"
    return 1
  fi
  if [[ -n "${GF2_ARDUINO_DEPS_SETUP_STATUS:-}" && "${GF2_ARDUINO_DEPS_SETUP_STATUS}" != "success" ]]; then
    mark_unavailable "${gate}" "remote Arduino dependency setup=${GF2_ARDUINO_DEPS_SETUP_STATUS}"
    return 1
  fi
  for tool in arduino-cli rsync; do
    if ! have_command "${tool}"; then
      mark_unavailable "${gate}" "missing prerequisite: ${tool}"
      return 1
    fi
  done

  if ! arduino-cli core list 2>/dev/null | grep -q '^m5stack:esp32[[:space:]]'; then
    mark_unavailable "${gate}" "m5stack:esp32 core not installed"
    return 1
  fi
  local libs
  libs="$(arduino-cli lib list 2>/dev/null || true)"
  for lib in M5Cardputer M5Unified M5GFX; do
    if ! grep -q "^${lib}[[:space:]]" <<<"${libs}"; then
      mark_unavailable "${gate}" "Arduino library not installed: ${lib}"
      return 1
    fi
  done
  return 0
}

run_cardputer() {
  if [[ "${REQUESTED[CARDPUTER_ADV]}" != 1 ]]; then return; fi
  if ! arduino_prerequisites_available CARDPUTER_ADV; then return; fi
  run_gate_command CARDPUTER_ADV "bash scripts/build.sh --warnings all" \
    bash "${ROOT}/scripts/build.sh" --warnings all
}

run_dram() {
  if [[ "${REQUESTED[FIXED_DRAM]}" != 1 ]]; then return; fi
  local elf="${ROOT}/build/cardputer-adv-current/GroovePuter.ino.elf"

  if [[ "${REQUESTED[CARDPUTER_ADV]}" == 1 && "${STATUS[CARDPUTER_ADV]}" != "PASS" ]]; then
    mark_unavailable FIXED_DRAM "Cardputer ADV build did not PASS in this validation run"
    return
  fi
  if [[ ! -f "${elf}" ]]; then
    mark_unavailable FIXED_DRAM "Cardputer ADV ELF unavailable: ${elf}"
    return
  fi

  run_gate_command FIXED_DRAM "check_cardputer_dram_budget.sh current Cardputer ELF" \
    bash "${ROOT}/scripts/check_cardputer_dram_budget.sh" "${elf}"
}

run_seqtrak() {
  if [[ "${REQUESTED[SEQTRAK_MIDI_ONLY]}" != 1 ]]; then return; fi
  if ! arduino_prerequisites_available SEQTRAK_MIDI_ONLY; then return; fi
  run_gate_command SEQTRAK_MIDI_ONLY "bash scripts/build_seqtrak_midi_only.sh --warnings all" \
    bash "${ROOT}/scripts/build_seqtrak_midi_only.sh" --warnings all
}

run_host
run_sdl
run_cardputer
run_dram
run_seqtrak

COMMIT_SHA="unknown"
if have_command git && git -C "${ROOT}" rev-parse HEAD >/dev/null 2>&1; then
  COMMIT_SHA="$(git -C "${ROOT}" rev-parse HEAD)"
fi

MATRIX_TXT="${REPORT_DIR}/matrix.txt"
MATRIX_JSON="${REPORT_DIR}/matrix.json"

{
  printf 'GF2 COMMIT            %s\n' "${COMMIT_SHA}"
  for gate in "${GATES[@]}"; do
    printf '%-22s %s\n' "${gate}" "${STATUS[${gate}]}"
  done
  printf '%-22s %s\n' "HARDWARE" "NOT TESTED"
} > "${MATRIX_TXT}"

json_escape() {
  local value="$1"
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  value=${value//$'\n'/\\n}
  printf '%s' "${value}"
}

{
  printf '{\n'
  printf '  "commit": "%s",\n' "$(json_escape "${COMMIT_SHA}")"
  printf '  "gates": {\n'
  local_index=0
  for gate in "${GATES[@]}"; do
    ((local_index += 1))
    comma=','
    if (( local_index == ${#GATES[@]} )); then comma=''; fi
    printf '    "%s": {"status": "%s", "detail": "%s"}%s\n' \
      "${gate}" \
      "$(json_escape "${STATUS[${gate}]}")" \
      "$(json_escape "${DETAIL[${gate}]}")" \
      "${comma}"
  done
  printf '  },\n'
  printf '  "hardware": "NOT TESTED"\n'
  printf '}\n'
} > "${MATRIX_JSON}"

cat "${MATRIX_TXT}"

overall=0
for gate in "${GATES[@]}"; do
  if [[ "${REQUESTED[${gate}]}" != 1 ]]; then continue; fi
  case "${STATUS[${gate}]}" in
    FAIL) overall=1 ;;
    UNAVAILABLE)
      if (( overall == 0 )); then overall=2; fi
      ;;
  esac
done

if (( overall == 0 )); then
  echo "GF2 TARGET STATUS     GREEN"
elif (( overall == 1 )); then
  echo "GF2 TARGET STATUS     NOT GREEN (FAIL)"
else
  echo "GF2 TARGET STATUS     NOT GREEN (UNAVAILABLE)"
fi

exit "${overall}"
