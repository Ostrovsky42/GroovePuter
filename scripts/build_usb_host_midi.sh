#!/usr/bin/env bash
set -euo pipefail

# Build GroovePuter with CDCOnBoot=default (disabled CDC auto-start).
# This configuration leaves the USB-OTG controller completely uninitialized
# at boot so that CardputerUsbRoleRuntime can select Host or Device at setup().
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

export FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc}"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-host-midi}"
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS:-} -DGROOVEPUTER_DEFAULT_USB_ROLE_HOST"

echo "=== Building GroovePuter Unified Boot-Role (CDCOnBoot=default + FS1B Dynbuffers) ==="
bash "${SCRIPT_DIR}/build_cardputer_dynbuffers.sh" "$@"
