#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Embedded USB hosts are often stricter than desktop operating systems. This
# profile exposes only the class-compliant MIDI interface, without CDC.
export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-seqtrak-midi-only}"

"${SCRIPT_DIR}/build.sh" "$@"
"${SCRIPT_DIR}/check_cardputer_dram_budget.sh" \
  "${BUILD_PATH}/GroovePuter.ino.elf"
