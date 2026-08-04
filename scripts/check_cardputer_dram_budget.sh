#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?usage: check_cardputer_dram_budget.sh <firmware.elf> [max-bytes]}"
MAX_BYTES="${2:-122880}"

if [[ ! -f "${ELF_PATH}" ]]; then
  echo "Firmware ELF not found: ${ELF_PATH}" >&2
  exit 2
fi

PKG_DIR="${ARDUINO_DATA_DIR:-}"
if [[ -z "${PKG_DIR}" ]] && command -v arduino-cli >/dev/null 2>&1; then
  PKG_DIR="$(arduino-cli config get directories.data 2>/dev/null || true)"
fi
if [[ -z "${PKG_DIR}" ]]; then
  PKG_DIR="${HOME}/.arduino15"
fi

TOOL_DIR=""
if [[ -d "${PKG_DIR}/packages/esp32/tools" ]]; then
  TOOL_DIR="$(find "${PKG_DIR}/packages/esp32/tools" \
    -type f -name 'xtensa-esp32s3-elf-size' -printf '%h\n' 2>/dev/null \
    | sort -V | tail -n 1 || true)"
fi
SIZE_TOOL="${SIZE_TOOL:-${TOOL_DIR:+${TOOL_DIR}/xtensa-esp32s3-elf-size}}"

echo "Firmware ELF: ${ELF_PATH}"
echo "Size tool: ${SIZE_TOOL:-<not found>}"

if [[ -z "${SIZE_TOOL}" || ! -x "${SIZE_TOOL}" ]]; then
  echo "xtensa-esp32s3-elf-size was not found under ${PKG_DIR}/packages/esp32/tools" >&2
  exit 2
fi

if [[ -z "${TOOL_DIR}" ]]; then
  TOOL_DIR="$(dirname "${SIZE_TOOL}")"
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
echo "  .dram0.data: ${DRAM_DATA} bytes"
echo "  .dram0.bss:  ${DRAM_BSS} bytes"
if (( DRAM_TOTAL > MAX_BYTES )); then
  echo "DRAM budget exceeded by $((DRAM_TOTAL - MAX_BYTES)) bytes" >&2

  NM_TOOL="${NM_TOOL:-${TOOL_DIR:+${TOOL_DIR}/xtensa-esp32s3-elf-nm}}"
  if [[ -n "${NM_TOOL}" && -x "${NM_TOOL}" ]]; then
    echo "Largest DRAM/BSS symbols:" >&2
    "${NM_TOOL}" --print-size --size-sort --radix=d "${ELF_PATH}" \
      | awk '$3 ~ /^[BbDd]$/ { printf "  %8s  %s  %s\n", $2, $3, $4 }' \
      | tail -n 30 >&2
  else
    echo "xtensa-esp32s3-elf-nm was not found; symbol diagnostics unavailable" >&2
  fi
  exit 1
fi
