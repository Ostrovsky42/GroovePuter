#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Error-level driver logs distinguish SPI/card protocol/FAT/VFS failures.
# Preserve product startup and playback; no automatic test notes or SD writes.
export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc,DebugLevel=error"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-recovery-diagnostics}"
bash "${SCRIPT_DIR}/build.sh" "$@"
bash "${SCRIPT_DIR}/check_cardputer_dram_budget.sh" \
  "${BUILD_PATH}/GroovePuter.ino.elf"
