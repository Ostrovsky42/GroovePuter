#!/bin/bash
# Upload script for launcher.bin on M5Stack Cardputer
# Cardputer uses StampS3 (ESP32-S3FN8) - NO PSRAM.

PORT="/dev/ttyACM0"
INPUT_FILE="./scripts/launcher.bin"
FLASH_ADDR="0x10000"
USED_DEFAULT_INPUT=1

for arg in "$@"; do
    case "$arg" in
        /dev/*) PORT="$arg" ;;
        *.bin) INPUT_FILE="$arg"; USED_DEFAULT_INPUT=0 ;;
        0x*) FLASH_ADDR="$arg" ;;
    esac
done

if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Binary file '$INPUT_FILE' not found!"
    echo "Usage: $0 [port] [path/to/launcher.bin|launcher.merged.bin] [flash_address]"
    exit 1
fi

# Auto-detect merged images unless caller explicitly set an address.
if [[ "$FLASH_ADDR" == "0x10000" && "$INPUT_FILE" == *.merged.bin ]]; then
    FLASH_ADDR="0x0"
fi

echo "Uploading binary: $INPUT_FILE to $PORT at $FLASH_ADDR..."
if [ "$USED_DEFAULT_INPUT" -eq 1 ]; then
    echo "WARNING: No .bin argument supplied."
    echo "         Defaulting to launcher image: $INPUT_FILE"
    echo "         If you want MiniAcid app, pass release_bins/grooveputer_latest.bin"
    echo "         or release_bins/grooveputer_latest.merged.bin explicitly."
fi

if command -v esptool >/dev/null 2>&1; then
    ESPTOOL_CMD=(esptool)
elif command -v esptool.py >/dev/null 2>&1; then
    ESPTOOL_CMD=(esptool.py)
elif python3 -c 'import esptool' >/dev/null 2>&1; then
    ESPTOOL_CMD=(python3 -m esptool)
elif [ -x "$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool" ]; then
    ESPTOOL_CMD=("$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool")
elif [ -f "$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool.py" ]; then
    ESPTOOL_CMD=(python3 "$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool.py")
else
    echo "Error: esptool not found."
    echo "Install one of: esptool, esptool.py, or Python module esptool."
    echo "Debian/Ubuntu example: pipx install esptool"
    exit 1
fi

"${ESPTOOL_CMD[@]}" --chip esp32s3 --port "$PORT" --baud 921600 \
    --before default_reset --after hard_reset \
    write_flash -z "$FLASH_ADDR" "$INPUT_FILE"
