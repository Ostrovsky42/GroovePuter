#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?usage: check_cardputer_dram_budget.sh <firmware.elf> [max-bytes]}"
# Provisional rollback to the last repository ceiling that predates the
# undocumented 122880-byte replacement. This is not a universal hardware
# safety boundary. It is an explicit policy exception: current evidence meets
# threshold-rule identity/static-profile items 1-4, while hardware minima,
# declared reserves, and the deriving calculation in items 5-7 remain pending.
MAX_BYTES="${2:-191488}"

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

read -r DRAM_DATA DRAM_BSS < <(
  "${SIZE_TOOL}" -A "${ELF_PATH}" | awk '
    $1 == ".dram0.data" { data = $2 }
    $1 == ".dram0.bss" { bss = $2 }
    END { print data + 0, bss + 0 }
  '
)
DRAM_TOTAL=$((DRAM_DATA + DRAM_BSS))

echo "Cardputer DRAM globals: ${DRAM_TOTAL} bytes (budget ${MAX_BYTES})"
echo "  .dram0.data: ${DRAM_DATA} bytes"
echo "  .dram0.bss:  ${DRAM_BSS} bytes"
echo "  policy: provisional exception; threshold-rule items 5-7 pending"
if (( DRAM_TOTAL > MAX_BYTES )); then
  echo "DRAM budget exceeded by $((DRAM_TOTAL - MAX_BYTES)) bytes" >&2

  if NM_TOOL="$(discover_arduino_tool xtensa-esp32s3-elf-nm "${NM_TOOL:-}" 2>/dev/null)"; then
    echo "Largest DRAM/BSS symbols:" >&2
    "${NM_TOOL}" --print-size --size-sort --radix=d "${ELF_PATH}" \
      | awk '$3 ~ /^[BbDd]$/ { printf "  %8s  %s  %s\n", $2, $3, $4 }' \
      | tail -n 30 >&2
  else
    echo "xtensa-esp32s3-elf-nm was not found; symbol diagnostics unavailable" >&2
  fi
  exit 1
fi
