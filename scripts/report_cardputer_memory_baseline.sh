#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?usage: report_cardputer_memory_baseline.sh <firmware.elf>}"
CURRENT_GATE_BYTES="${CURRENT_GATE_BYTES:-122880}"
PREVIOUS_GATE_BYTES="${PREVIOUS_GATE_BYTES:-191488}"

if [[ ! -f "${ELF_PATH}" ]]; then
  echo "Firmware ELF not found: ${ELF_PATH}" >&2
  exit 2
fi

TOOL_DIR="$(find "${HOME}/.arduino15/packages/esp32/tools" \
  -type f -name 'xtensa-esp32s3-elf-size' -printf '%h\n' 2>/dev/null \
  | sort -V | tail -n 1)"
SIZE_TOOL="${SIZE_TOOL:-${TOOL_DIR:+${TOOL_DIR}/xtensa-esp32s3-elf-size}}"
NM_TOOL="${NM_TOOL:-${TOOL_DIR:+${TOOL_DIR}/xtensa-esp32s3-elf-nm}}"

if [[ -z "${SIZE_TOOL}" || ! -x "${SIZE_TOOL}" ]]; then
  echo "xtensa-esp32s3-elf-size was not found" >&2
  exit 2
fi
if [[ -z "${NM_TOOL}" || ! -x "${NM_TOOL}" ]]; then
  echo "xtensa-esp32s3-elf-nm was not found" >&2
  exit 2
fi

read -r DRAM_DATA DRAM_BSS < <(
  "${SIZE_TOOL}" -A "${ELF_PATH}" | awk '
    $1 == ".dram0.data" { data = $2 }
    $1 == ".dram0.bss" { bss = $2 }
    END { print data + 0, bss + 0 }
  '
)
DRAM_TOTAL=$((DRAM_DATA + DRAM_BSS))
CURRENT_DELTA=$((CURRENT_GATE_BYTES - DRAM_TOTAL))
PREVIOUS_DELTA=$((PREVIOUS_GATE_BYTES - DRAM_TOTAL))

printf '%s\n' "=== Cardputer memory baseline ==="
printf 'ELF: %s\n' "${ELF_PATH}"
printf '.dram0.data: %d bytes\n' "${DRAM_DATA}"
printf '.dram0.bss:  %d bytes\n' "${DRAM_BSS}"
printf 'fixed DRAM:  %d bytes\n' "${DRAM_TOTAL}"
printf 'current gate:  %d bytes (delta %+d)\n' \
  "${CURRENT_GATE_BYTES}" "${CURRENT_DELTA}"
printf 'previous gate: %d bytes (delta %+d)\n' \
  "${PREVIOUS_GATE_BYTES}" "${PREVIOUS_DELTA}"

NM_OUTPUT="$(mktemp)"
cleanup() {
  rm -f "${NM_OUTPUT}"
}
trap cleanup EXIT

"${NM_TOOL}" --print-size --size-sort --radix=d "${ELF_PATH}" \
  | awk '$3 ~ /^[BbDd]$/ { print $1, $2, $3, $4 }' > "${NM_OUTPUT}"

print_matches() {
  local label="$1"
  shift
  local pattern
  local found=0
  local total=0
  printf '\n%s:\n' "${label}"
  for pattern in "$@"; do
    while read -r address size type name; do
      [[ -n "${name:-}" ]] || continue
      if [[ "${name}" == *"${pattern}"* ]]; then
        printf '  %8d  %s  %s\n' "${size}" "${type}" "${name}"
        total=$((total + size))
        found=1
      fi
    done < "${NM_OUTPUT}"
  done
  if (( found == 0 )); then
    echo "  not found"
  else
    printf '  subtotal: %d bytes\n' "${total}"
  fi
}

print_matches "Scene transaction scratch" "s_tempLoadScene"
print_matches "Active Scene" "g_mainScene"
print_matches "Wavetable static arrays" \
  "sineTable_" "squareTable_" "triangleTable_" "sawTable_"
print_matches "SMF player runtime" "g_smfPlayer"
print_matches "MiniAcid engine" "g_miniAcidInstance"

printf '\nLargest fixed DRAM symbols:\n'
tail -n 40 "${NM_OUTPUT}" \
  | awk '{ printf "  %8s  %s  %s\n", $2, $3, $4 }'

printf '\nMachine summary:\n'
printf 'MEMORY_BASELINE fixed=%d data=%d bss=%d current_gate=%d current_delta=%+d previous_gate=%d previous_delta=%+d\n' \
  "${DRAM_TOTAL}" "${DRAM_DATA}" "${DRAM_BSS}" \
  "${CURRENT_GATE_BYTES}" "${CURRENT_DELTA}" \
  "${PREVIOUS_GATE_BYTES}" "${PREVIOUS_DELTA}"

# This script is deliberately non-gating. The existing mandatory gate remains
# responsible for failing the product build until the baseline decision is made.
exit 0
