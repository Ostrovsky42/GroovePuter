#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-dynbuffers}"
UPLOAD_SPEED="${UPLOAD_SPEED:-921600}"
PORT="${1:-/dev/ttyACM0}"

if [[ $# -gt 1 || "${PORT}" != /dev/* ]]; then
  echo "Usage: $0 [/dev/ttyACM0]" >&2
  exit 2
fi

export FQBN ARDUINO_CLI BUILD_PATH
bash "${SCRIPT_DIR}/build_cardputer_dynbuffers.sh"
bash "${SCRIPT_DIR}/check_cardputer_dram_budget.sh" \
  "${BUILD_PATH}/GroovePuter.ino.elf"

if [[ ! -f "${BUILD_PATH}/GroovePuter.ino.bin" ]]; then
  echo "Current dynamic-buffer build is missing: ${BUILD_PATH}/GroovePuter.ino.bin" >&2
  exit 1
fi

echo "Uploading UI + FS1 dynamic-buffer candidate: ${BUILD_PATH}"
"${ARDUINO_CLI}" upload \
  --fqbn "${FQBN}" \
  -p "${PORT}" \
  --upload-property "upload.speed=${UPLOAD_SPEED}" \
  --input-dir "${BUILD_PATH}"
