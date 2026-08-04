#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?usage: report_cardputer_memory_baseline.sh <firmware.elf>}"
IMAGE_KIND="${MEMORY_BASELINE_IMAGE_KIND:-unspecified}"
PROVISIONAL_GATE_BYTES="${PROVISIONAL_GATE_BYTES:-191488}"
UNSUPPORTED_GATE_BYTES="${UNSUPPORTED_GATE_BYTES:-122880}"

if [[ ! -f "${ELF_PATH}" ]]; then
  echo "Firmware ELF not found: ${ELF_PATH}" >&2
  exit 2
fi

discover_arduino_tool() {
  local tool_name="$1"
  local override="${2:-}"
  local path_candidate=""
  local package_root="${ARDUINO_PACKAGES_ROOT:-${HOME}/.arduino15/packages}"
  local -a candidates=()

  if [[ -n "${override}" ]]; then
    if [[ -x "${override}" ]]; then
      printf '%s\n' "${override}"
      return 0
    fi
    echo "Configured ${tool_name} is not executable: ${override}" >&2
    return 2
  fi

  path_candidate="$(command -v "${tool_name}" 2>/dev/null || true)"
  if [[ -n "${path_candidate}" && -x "${path_candidate}" ]]; then
    printf '%s\n' "${path_candidate}"
    return 0
  fi

  if [[ -d "${package_root}" ]]; then
    mapfile -t candidates < <(
      find "${package_root}" -type f -name "${tool_name}" -print 2>/dev/null \
        | sort -V || true
    )
  fi
  if (( ${#candidates[@]} == 0 )); then
    echo "${tool_name} was not found in PATH or ${package_root}" >&2
    return 2
  fi

  path_candidate="${candidates[$((${#candidates[@]} - 1))]}"
  if [[ ! -x "${path_candidate}" ]]; then
    echo "Discovered ${tool_name} is not executable: ${path_candidate}" >&2
    return 2
  fi
  printf '%s\n' "${path_candidate}"
}

SIZE_TOOL="$(discover_arduino_tool xtensa-esp32s3-elf-size "${SIZE_TOOL:-}")"
NM_TOOL="$(discover_arduino_tool xtensa-esp32s3-elf-nm "${NM_TOOL:-}")"

read -r DRAM_DATA DRAM_BSS < <(
  "${SIZE_TOOL}" -A "${ELF_PATH}" | awk '
    $1 == ".dram0.data" { data = $2 }
    $1 == ".dram0.bss" { bss = $2 }
    END { print data + 0, bss + 0 }
  '
)
DRAM_TOTAL=$((DRAM_DATA + DRAM_BSS))
PROVISIONAL_HEADROOM=$((PROVISIONAL_GATE_BYTES - DRAM_TOTAL))
UNSUPPORTED_HEADROOM=$((UNSUPPORTED_GATE_BYTES - DRAM_TOTAL))

printf '%s\n' "=== Cardputer memory baseline ==="
printf 'image kind: %s\n' "${IMAGE_KIND}"
printf 'ELF: %s\n' "${ELF_PATH}"
printf '.dram0.data: %d bytes\n' "${DRAM_DATA}"
printf '.dram0.bss:  %d bytes\n' "${DRAM_BSS}"
printf 'fixed DRAM:  %d bytes\n' "${DRAM_TOTAL}"
printf 'provisional gate: %d bytes (headroom %+d)\n' \
  "${PROVISIONAL_GATE_BYTES}" "${PROVISIONAL_HEADROOM}"
printf 'unsupported 122880 reference: %d bytes (headroom %+d)\n' \
  "${UNSUPPORTED_GATE_BYTES}" "${UNSUPPORTED_HEADROOM}"

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
  local size_dec=0
  printf '\n%s:\n' "${label}"
  for pattern in "$@"; do
    while read -r address size type name; do
      [[ -n "${name:-}" ]] || continue
      if [[ "${name}" == *"${pattern}"* ]]; then
        size_dec=$((10#${size}))
        printf '  %8d  %s  %s\n' "${size_dec}" "${type}" "${name}"
        total=$((total + size_dec))
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
  | awk '{ size = $2 + 0; printf "  %8d  %s  %s\n", size, $3, $4 }'

printf '\nMachine summary:\n'
printf 'MEMORY_BASELINE kind=%s fixed=%d data=%d bss=%d provisional_gate=%d provisional_headroom=%+d unsupported_gate=%d unsupported_headroom=%+d\n' \
  "${IMAGE_KIND}" "${DRAM_TOTAL}" "${DRAM_DATA}" "${DRAM_BSS}" \
  "${PROVISIONAL_GATE_BYTES}" "${PROVISIONAL_HEADROOM}" \
  "${UNSUPPORTED_GATE_BYTES}" "${UNSUPPORTED_HEADROOM}"

# This script is deliberately non-gating. The mandatory gate is restored only
# to the provisional pre-122880 ceiling until profile-specific runtime evidence
# supports a replacement policy.
exit 0
