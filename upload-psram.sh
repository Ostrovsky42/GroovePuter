#!/bin/bash
# Upload script for MiniAcid on M5Stack Cardputer
# Cardputer uses StampS3 (ESP32-S3FN8) - NO PSRAM

FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app"
PORT="${1:-/dev/ttyACM0}"

echo "=== Uploading MiniAcid to $PORT (Cardputer, DRAM-only) ==="
./arduino-cli upload --fqbn "$FQBN" -p "$PORT"
