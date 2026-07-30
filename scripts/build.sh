#!/usr/bin/env bash
set -euo pipefail

# Cardputer and Cardputer-Adv use the M5Stack M5Cardputer board option.
# GroovePuter targets the DRAM-only configuration and huge_app partition.
# USB-OTG/TinyUSB plus CDC-on-boot keeps Serial/upload available while adding
# a class-compliant USB-MIDI interface to the native ESP32-S3 USB port.
FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"

if ! command -v "${ARDUINO_CLI}" >/dev/null 2>&1; then
  echo "arduino-cli was not found: ${ARDUINO_CLI}" >&2
  echo "Install it or set ARDUINO_CLI=/absolute/path/to/arduino-cli" >&2
  exit 127
fi

echo "=== Building GroovePuter for Cardputer ADV ==="
echo "FQBN: ${FQBN}"
"${ARDUINO_CLI}" compile --fqbn "${FQBN}" . "$@"
