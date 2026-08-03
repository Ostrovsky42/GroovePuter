#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?usage: check_cardputer_dram_budget.sh <firmware.elf> [max-bytes]}"
MAX_BYTES="${2:-191488}"

if [[ ! -f "${ELF_PATH}" ]]; then
  echo "Firmware ELF not found: ${ELF_PATH}" >&2
  exit 2
fi

SIZE_TOOL="${SIZE_TOOL:-}"
if [[ -z "${SIZE_TOOL}" ]]; then
  SIZE_TOOL="$(find "${HOME}/.arduino15/packages/esp32/tools" \
    -type f -name 'xtensa-esp32s3-elf-size' -print 2>/dev/null \
    | sort -V | tail -n 1)"
fi
if [[ -z "${SIZE_TOOL}" || ! -x "${SIZE_TOOL}" ]]; then
  echo "xtensa-esp32s3-elf-size was not found" >&2
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

echo "Cardputer DRAM globals: ${DRAM_TOTAL} bytes (budget ${MAX_BYTES})"
if (( DRAM_TOTAL > MAX_BYTES )); then
  echo "DRAM budget exceeded by $((DRAM_TOTAL - MAX_BYTES)) bytes" >&2
  exit 1
fi
