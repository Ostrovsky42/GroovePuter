#!/bin/bash
# Upload script for GroovePuter on M5Stack Cardputer-Adv.
# Cardputer-Adv uses Stamp-S3A / ESP32-S3FN8 and no PSRAM.
# Default behavior: upload CURRENT sources.
# Optional: pass --prebuilt to flash release_bins/miniacid.ino.bin.

FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
PORT="/dev/ttyACM0"
USE_PREBUILT=0

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
    arduino-cli upload --fqbn "$FQBN" -p "$PORT" --input-file "release_bins/miniacid.ino.bin"
else
    echo "Using current source build path..."
    arduino-cli upload --fqbn "$FQBN" -p "$PORT"
fi
