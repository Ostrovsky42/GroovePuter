#!/bin/bash
set -e

# Configuration
# Aligning with build.sh: using huge_app (3MB) for better compatibility with audio projects
# and disabling PSRAM as a safe baseline for boot troubleshooting.
# FlashMode=dio chosen to match the ROM detection seen in Serial logs.
FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,FlashMode=dio"
OUT_DIR="release_bins"
APP_NAME="grooveputer"
TIMESTAMP=$(date +"%H%MI%m%d")
BIN_NAME="${APP_NAME}_${TIMESTAMP}.bin"
ARDUINO_CLI="./platform_sdl/bin/arduino-cli"

echo "=== Building MiniAcid Release (Stable Config: No PSRAM, Huge APP, DIO) ==="
echo "FQBN: $FQBN"
echo "Output: $OUT_DIR/$BIN_NAME"

# Ensure output dir exists
mkdir -p "$OUT_DIR"

# Build phase
echo "[BUILD] Compiling sketch... (Incremental)"
# Removed --clean for speed
$ARDUINO_CLI compile --fqbn "$FQBN" --output-dir "$OUT_DIR" .

# Normalize output names regardless of sketch filename
# arduino-cli with --output-dir puts files as <sketch_name>.ino.bin
APP_BIN_SRC=""
for candidate in "$OUT_DIR"/*.ino.bin; do
    if [ -f "$candidate" ]; then
        APP_BIN_SRC="$candidate"
        break
    fi
done

if [ -n "$APP_BIN_SRC" ]; then
    cp -f "$APP_BIN_SRC" "$OUT_DIR/$BIN_NAME"
    # Create a 'latest' symlink or copy for convenience
    cp -f "$APP_BIN_SRC" "$OUT_DIR/${APP_NAME}_latest.bin"
    echo "Success! Application binary created at: $OUT_DIR/$BIN_NAME"
    echo "Latest link: $OUT_DIR/${APP_NAME}_latest.bin"
else
    echo "Error: Application binary not found!"
    exit 1
fi
