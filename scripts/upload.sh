#!/usr/bin/env bash
set -euo pipefail
# Upload script for GroovePuter on M5Stack Cardputer-Adv.
# Cardputer-Adv uses Stamp-S3A / ESP32-S3FN8 and no PSRAM.
# Default behavior: upload CURRENT sources.
# Optional: pass --prebuilt to flash release_bins/miniacid.ino.bin.

FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-current}"
PORT="/dev/ttyACM0"
USE_PREBUILT=0
UPLOAD_SPEED="${UPLOAD_SPEED:-921600}"

for arg in "$@"; do
    case "$arg" in
        --prebuilt) USE_PREBUILT=1 ;;
        /dev/*) PORT="$arg" ;;
        *) echo "Unknown argument: $arg" >&2; exit 2 ;;
    esac
done

if [ "$USE_PREBUILT" -eq 1 ]; then
    PREBUILT_PATH="${PROJECT_ROOT}/release_bins/miniacid.ino.bin"
    if [ ! -f "${PREBUILT_PATH}" ]; then
        echo "Pre-built binary is missing: ${PREBUILT_PATH}" >&2
        exit 1
    fi
    # arduino-cli upload expects sibling *.bootloader.bin and *.partitions.bin
    # with the same base name, so use the canonical compile artifact.
    echo "Using pre-built binary: ${PREBUILT_PATH}"
    "${ARDUINO_CLI}" upload --fqbn "$FQBN" -p "$PORT" \
        --upload-property "upload.speed=${UPLOAD_SPEED}" \
        --input-file "${PREBUILT_PATH}"
else
    # An existing .bin says nothing about the checkout or USB profile. Build
    # with the exact upload configuration, and stop on any build/gate failure
    # even when an older artifact is still present in the output directory.
    export FQBN ARDUINO_CLI BUILD_PATH
    bash "${SCRIPT_DIR}/build.sh"
    bash "${SCRIPT_DIR}/check_cardputer_dram_budget.sh" \
        "${BUILD_PATH}/GroovePuter.ino.elf"

    if [ ! -f "${BUILD_PATH}/GroovePuter.ino.bin" ]; then
        echo "Current build is missing: ${BUILD_PATH}/GroovePuter.ino.bin" >&2
        echo "Run: bash scripts/build.sh" >&2
        exit 1
    fi

    echo "Uploading current build: ${BUILD_PATH}"
    "${ARDUINO_CLI}" upload --fqbn "$FQBN" -p "$PORT" \
        --upload-property "upload.speed=${UPLOAD_SPEED}" \
        --input-dir "${BUILD_PATH}"
fi
