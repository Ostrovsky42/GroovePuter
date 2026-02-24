#!/bin/bash
set -e

# Configuration
# Keep this exactly aligned with scripts/build.sh (known-good runtime on hardware).
# OLD (caused mismatch vs regular upload path):
# FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,FlashMode=dio"
FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app"
OUT_DIR="release_bins"
APP_NAME="grooveputer"
TIMESTAMP=$(date +"%H%MI%m%d")
BIN_NAME="${APP_NAME}_${TIMESTAMP}.bin"
MERGED_NAME="${APP_NAME}_${TIMESTAMP}.merged.bin"
ARDUINO_CLI="./platform_sdl/bin/arduino-cli"

echo "=== Building MiniAcid Dev Release (No PSRAM, Huge APP) ==="
echo "FQBN: $FQBN"
echo "Output: $OUT_DIR/$BIN_NAME"

# Ensure output dir exists
mkdir -p "$OUT_DIR"

# Build phase
echo "[BUILD] Compiling sketch... (--clean for deterministic binaries)"
# OLD fast path (kept for reference):
# $ARDUINO_CLI compile --fqbn "$FQBN" --output-dir "$OUT_DIR" .
$ARDUINO_CLI compile --clean --fqbn "$FQBN" --output-dir "$OUT_DIR" .

# Normalize output names regardless of sketch filename
APP_BIN_SRC="$OUT_DIR/miniacid.ino.bin"
MERGED_BIN_SRC="$OUT_DIR/miniacid.ino.merged.bin"

if [ -f "$APP_BIN_SRC" ]; then
    cp -f "$APP_BIN_SRC" "$OUT_DIR/$BIN_NAME"
    cp -f "$APP_BIN_SRC" "$OUT_DIR/${APP_NAME}_latest.bin"
    echo "Success! App binary: $OUT_DIR/$BIN_NAME"
    echo "Latest app:   $OUT_DIR/${APP_NAME}_latest.bin"
else
    echo "Error: Application binary not found!"
    exit 1
fi

if [ -f "$MERGED_BIN_SRC" ]; then
    cp -f "$MERGED_BIN_SRC" "$OUT_DIR/$MERGED_NAME"
    cp -f "$MERGED_BIN_SRC" "$OUT_DIR/${APP_NAME}_latest.merged.bin"
    echo "Merged binary: $OUT_DIR/$MERGED_NAME"
    echo "Latest merged: $OUT_DIR/${APP_NAME}_latest.merged.bin"
fi
