#!/bin/bash
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
    esac
done

if [ "$USE_PREBUILT" -eq 1 ] && [ -f "release_bins/miniacid.ino.bin" ]; then
    # arduino-cli upload expects sibling *.bootloader.bin and *.partitions.bin
    # with the same base name, so use the canonical compile artifact.
    echo "Using pre-built binary: release_bins/miniacid.ino.bin"
    "${ARDUINO_CLI}" upload --fqbn "$FQBN" -p "$PORT" \
        --upload-property "upload.speed=${UPLOAD_SPEED}" \
        --input-file "release_bins/miniacid.ino.bin"
else
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
