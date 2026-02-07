#!/bin/bash
set -e

# Configuration
# Aligning with build.sh: using huge_app (3MB) for better compatibility with audio projects
# and disabling PSRAM as a safe baseline for boot troubleshooting.
# FlashMode=dio chosen to match the ROM detection seen in Serial logs.
FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,FlashMode=dio"
OUT_DIR="dist"
BIN_NAME="miniacid.bin"
MERGED_BIN="miniacid.merged.bin"

echo "=== Building MiniAcid Release (Stable Config: No PSRAM, Huge APP, DIO) ==="
echo "FQBN: $FQBN"
echo "Output: $OUT_DIR/$BIN_NAME"

# Ensure output dir exists
mkdir -p "$OUT_DIR"

# Build phase
echo "[BUILD] Compiling sketch..."
./arduino-cli compile --clean --fqbn "$FQBN" --output-dir "$OUT_DIR" .

# Standard naming
if [ -f "$OUT_DIR/miniacid.ino.bin" ]; then
    mv "$OUT_DIR/miniacid.ino.bin" "$OUT_DIR/$BIN_NAME"
fi

# Merged binary creation (Application + Bootloader + Partition Table)
# This resolve partition/segment mismatch issues by flashing the whole image at 0x0.
if [ -f "$OUT_DIR/miniacid.ino.merged.bin" ]; then
    mv "$OUT_DIR/miniacid.ino.merged.bin" "$OUT_DIR/$MERGED_BIN"
    echo "Success! Combined binary created at: $OUT_DIR/$MERGED_BIN"
fi

echo "Success! Application binary created at: $OUT_DIR/$BIN_NAME"
echo "Partition scheme used: huge_app (3MB APP / 1MB SPIFFS)"
