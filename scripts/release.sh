#!/bin/bash
set -e

# Configuration
# Align with scripts/build.sh (known-good runtime on hardware).
# OLD (kept for reference): FlashMode override path
# FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,FlashMode=dio"
FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app"
OUT_DIR="release_bins"
APP_NAME="grooveputer"
BIN_NAME="${APP_NAME}.bin"
MERGED_BIN="${APP_NAME}.merged.bin"
ARDUINO_CLI="./platform_sdl/bin/arduino-cli"

echo "=== Building MiniAcid Release (No PSRAM, Huge APP) ==="
echo "FQBN: $FQBN"
echo "Output: $OUT_DIR/$BIN_NAME"

# Ensure output dir exists
mkdir -p "$OUT_DIR"

# Build phase
echo "[BUILD] Compiling sketch..."
$ARDUINO_CLI compile --clean --fqbn "$FQBN" --output-dir "$OUT_DIR" .

# Normalize output names regardless of sketch filename
# OLD glob-based detection (kept for reference):
# APP_BIN_SRC=""
# MERGED_BIN_SRC=""
# for candidate in "$OUT_DIR"/*.ino.bin; do
#     if [ -f "$candidate" ]; then
#         APP_BIN_SRC="$candidate"
#         break
#     fi
# done
# for candidate in "$OUT_DIR"/*.ino.merged.bin; do
#     if [ -f "$candidate" ]; then
#         MERGED_BIN_SRC="$candidate"
#         break
#     fi
# done
APP_BIN_SRC="$OUT_DIR/miniacid.ino.bin"
MERGED_BIN_SRC="$OUT_DIR/miniacid.ino.merged.bin"

if [ -n "$APP_BIN_SRC" ]; then
    cp -f "$APP_BIN_SRC" "$OUT_DIR/$BIN_NAME"
fi

# Merged binary creation (Application + Bootloader + Partition Table)
# This resolve partition/segment mismatch issues by flashing the whole image at 0x0.
if [ -n "$MERGED_BIN_SRC" ]; then
    cp -f "$MERGED_BIN_SRC" "$OUT_DIR/$MERGED_BIN"
    echo "Success! Combined binary created at: $OUT_DIR/$MERGED_BIN"
fi

echo "Success! Application binary created at: $OUT_DIR/$BIN_NAME"
