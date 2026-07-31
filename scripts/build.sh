#!/usr/bin/env bash
set -euo pipefail

# Cardputer and Cardputer-Adv use the M5Stack M5Cardputer board option.
# GroovePuter targets the DRAM-only configuration and huge_app partition.
# USB-OTG/TinyUSB plus CDC-on-boot keeps Serial/upload available while adding
# a class-compliant USB-MIDI interface to the native ESP32-S3 USB port.
FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-current}"

if ! command -v "${ARDUINO_CLI}" >/dev/null 2>&1; then
  echo "arduino-cli was not found: ${ARDUINO_CLI}" >&2
  echo "Install it or set ARDUINO_CLI=/absolute/path/to/arduino-cli" >&2
  exit 127
fi

echo "=== Building GroovePuter for Cardputer ADV ==="
echo "FQBN: ${FQBN}"
echo "Output: ${BUILD_PATH}"

# Arduino CLI requires the sketch directory name to match GroovePuter.ino.
# The repository is named miniacid, so stage the current checkout under the
# required directory name without changing the user's branch or files.
TEMP_ROOT="$(mktemp -d /tmp/grooveputer-build.XXXXXX)"
cleanup() {
  rm -rf "${TEMP_ROOT}"
}
trap cleanup EXIT

mkdir -p "${TEMP_ROOT}/GroovePuter"
mkdir -p "${BUILD_PATH}"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'platform_sdl/build' \
  "${PROJECT_ROOT}/" "${TEMP_ROOT}/GroovePuter/"

"${ARDUINO_CLI}" compile \
  --clean \
  --fqbn "${FQBN}" \
  --output-dir "${BUILD_PATH}" \
  "$@" \
  "${TEMP_ROOT}/GroovePuter"
