#!/bin/bash
# Build script for MiniAcid on M5Stack Cardputer
# Cardputer uses StampS3 (ESP32-S3FN8) - NO PSRAM

FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app"

echo "=== Building MiniAcid for Cardputer (DRAM-only, no PSRAM) ==="
./arduino-cli compile --fqbn "$FQBN" "$@"
