#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-seqtrak-midi-only}"
export UPLOAD_SPEED="${UPLOAD_SPEED:-115200}"

cat <<EOF
Uploading the SEQTRAK MIDI-only profile at ${UPLOAD_SPEED} baud. The running firmware will not expose
a CDC serial port. To restore the normal diagnostic build, enter download mode:
power OFF, hold G0, connect/power ON, release G0, then run scripts/upload.sh.
EOF

exec "${SCRIPT_DIR}/upload.sh" "$@"
