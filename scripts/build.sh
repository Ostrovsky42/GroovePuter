#!/bin/bash
# Build script for MiniAcid on M5Stack Cardputer
# NOTE: Cardputer uses StampS3 (ESP32-S3FN8) which has NO PSRAM
# The project is optimized to run efficiently in DRAM only

FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app"

ARDUINO_CLI="./platform_sdl/bin/arduino-cli"

echo "=== Building MiniAcid for Cardputer (DRAM-only) ==="
$ARDUINO_CLI compile --fqbn "$FQBN" . "$@"
