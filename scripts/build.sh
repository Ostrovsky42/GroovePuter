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
ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"
# Genre materialization keeps several transactional pattern copies alive in
# the Arduino loop task. The ESP32 core defaults that task to 8 KiB, which is
# insufficient on the DRAM-only Cardputer build and resets the device when
# generation materializes a genre or synth pattern. The current Stage 15
# generator has over 7 KiB of nested frames before the Arduino loop/UI frames,
# so reserve 32 KiB for the control/UI task. Keep the audio task unchanged.
ARDUINO_LOOP_STACK_SIZE="${ARDUINO_LOOP_STACK_SIZE:-32768}"
GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS:-}"

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
mkdir -p "${ARDUINO_BUILD_PATH}"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'platform_sdl/build' \
  "${PROJECT_ROOT}/" "${TEMP_ROOT}/GroovePuter/"

# Arduino CLI may compile nested sketch sources from generated build paths.
# Keep the staged page directory on the include search path so quoted
# implementation fragments remain visible without compiling them separately.
ARDUINO_CPP_EXTRA_FLAGS="-DARDUINO_LOOP_STACK_SIZE=${ARDUINO_LOOP_STACK_SIZE} -I${TEMP_ROOT}/GroovePuter/src/ui/pages ${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS}"

"${ARDUINO_CLI}" compile \
  --clean \
  --fqbn "${FQBN}" \
  --build-path "${ARDUINO_BUILD_PATH}" \
  --build-property "compiler.cpp.extra_flags=${ARDUINO_CPP_EXTRA_FLAGS}" \
  --output-dir "${BUILD_PATH}" \
  "$@" \
  "${TEMP_ROOT}/GroovePuter"
