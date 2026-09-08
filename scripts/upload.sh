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
ALLOW_STOCK_FATFS=0
UPLOAD_SPEED="${UPLOAD_SPEED:-921600}"

for arg in "$@"; do
    case "$arg" in
        --prebuilt) USE_PREBUILT=1 ;;
        --stock-fatfs) ALLOW_STOCK_FATFS=1 ;;
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

    # A firmware linked against the stock libfatfs.a reboot-loops as soon as an
    # SD card is inserted: FatFs reserves 4096-byte sector buffers permanently,
    # the largest free DRAM block collapses to about 7.6 KB, and the device
    # panics in loop(). That is FS1, and the fix lives in
    # scripts/build_cardputer_dynbuffers.sh, not in the default build -- so the
    # default flashing path used to hand the user a device that dies on a card.
    #
    # The check is on the link map, not on which script ran, because that is the
    # thing that is actually true of the binary about to be written.
    MAP_FILE="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}/GroovePuter.ino.map"
    if [ "$ALLOW_STOCK_FATFS" -eq 0 ]; then
        if [ ! -f "${MAP_FILE}" ]; then
            echo "Cannot verify the FatFs build: link map not found (${MAP_FILE})" >&2
            echo "Build with: bash scripts/build_cardputer_dynbuffers.sh" >&2
            echo "Override only for a deliberate A/B: $0 --stock-fatfs" >&2
            exit 1
        fi
        if ! grep -Eq "(grooveputer-sdk-dynbuffers|fatfs-dynbuffers)/libfatfs\.a" "${MAP_FILE}"; then
            echo "REFUSING TO FLASH: this build uses the stock libfatfs.a." >&2
            echo "It reboot-loops with an SD card inserted (FS1)." >&2
            echo "Build with: bash scripts/build_cardputer_dynbuffers.sh" >&2
            echo "Override only for a deliberate A/B: $0 --stock-fatfs" >&2
            exit 1
        fi
    else
        echo "WARNING: flashing a stock-FatFs build on purpose." >&2
        echo "WARNING: this firmware reboot-loops with an SD card inserted." >&2
    fi

    echo "Uploading current build: ${BUILD_PATH}"
    "${ARDUINO_CLI}" upload --fqbn "$FQBN" -p "$PORT" \
        --upload-property "upload.speed=${UPLOAD_SPEED}" \
        --input-dir "${BUILD_PATH}"
fi
