#!/bin/bash
# Upload script for MiniAcid on M5Stack Cardputer
# Cardputer uses StampS3 (ESP32-S3FN8) - NO PSRAM.
# Default behavior: upload CURRENT sources.
# Optional: pass --prebuilt to flash release_bins/grooveputer_latest.bin.

FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app"
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
    # OLD (kept for reference, fails without matching companion files):
    # arduino-cli upload --fqbn "$FQBN" -p "$PORT" --input-file "release_bins/grooveputer_latest.bin"
    arduino-cli upload --fqbn "$FQBN" -p "$PORT" --input-file "release_bins/miniacid.ino.bin"
else
    echo "Using current source build path..."
    arduino-cli upload --fqbn "$FQBN" -p "$PORT"
fi
