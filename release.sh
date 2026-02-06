#!/bin/bash
set -e

# Configuration
# Switching to 'default' partition scheme instead of 'huge_app' to fix checksum/bootloader issues with M5Launcher
# FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=default"
# Build with OPI PSRAM enabled (Correct for Cardputer)
 FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=enabled,PartitionScheme=default"
OUT_DIR="dist"
BIN_NAME="miniacid.bin"

echo "=== Building MiniAcid Release (DRAM-only, Default Partitions) ==="
echo "FQBN: $FQBN"
echo "Output: $OUT_DIR/$BIN_NAME"

# Ensure output dir exists
mkdir -p "$OUT_DIR"

# Build
./arduino-cli compile --fqbn "$FQBN" --output-dir "$OUT_DIR" .

# Rename/Prepare for Launcher
if [ -f "$OUT_DIR/miniacid.ino.bin" ]; then
    mv "$OUT_DIR/miniacid.ino.bin" "$OUT_DIR/$BIN_NAME"
    echo "Success! Binary created at: $OUT_DIR/$BIN_NAME"
    echo "Partition scheme used: default (should be compatible with standard bootloaders)"
else
    echo "Error: Compilation failed or output file missing."
    exit 1
fi
