#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_NAME="${BUILD_NAME:-cardputer-adv-pr11}"
OUT_DIR="${PROJECT_ROOT}/build/${BUILD_NAME}"
ARDUINO_CLI="${ARDUINO_CLI:-${PROJECT_ROOT}/platform_sdl/bin/arduino-cli}"
FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
LAUNCHER_SIZE=1376256

cd "${PROJECT_ROOT}"

CURRENT_COMMIT="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
echo "[1/3] Source: current checkout (${CURRENT_COMMIT})"

TEMP_ROOT="$(mktemp -d /tmp/grooveputer-build.XXXXXX)"
cleanup() {
  rm -rf "${TEMP_ROOT}"
}
trap cleanup EXIT

# Arduino CLI expects the sketch directory name to match GroovePuter.ino.
mkdir -p "${TEMP_ROOT}/GroovePuter"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'platform_sdl/build' \
  "${PROJECT_ROOT}/" "${TEMP_ROOT}/GroovePuter/"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found or not executable: ${ARDUINO_CLI}" >&2
  exit 127
fi

mkdir -p "${OUT_DIR}"
echo "[2/3] Building Cardputer-Adv..."
"${ARDUINO_CLI}" compile \
  --clean \
  --fqbn "${FQBN}" \
  --output-dir "${OUT_DIR}" \
  "${TEMP_ROOT}/GroovePuter"

RAW_BIN="${OUT_DIR}/GroovePuter.ino.bin"
LAUNCHER_BIN="${OUT_DIR}/GP-PR11-MIDI-COMPANION.bin"
if [[ ! -f "${RAW_BIN}" ]]; then
  echo "Build succeeded but application binary is missing: ${RAW_BIN}" >&2
  exit 1
fi

echo "[3/3] Creating launcher image..."
cp "${RAW_BIN}" "${LAUNCHER_BIN}"
truncate -s "${LAUNCHER_SIZE}" "${LAUNCHER_BIN}"

echo ""
echo "Application: ${RAW_BIN}"
echo "Launcher:   ${LAUNCHER_BIN}"
echo "Size:       $(stat -c '%s bytes' "${LAUNCHER_BIN}")"
echo "SHA-256:    $(sha256sum "${LAUNCHER_BIN}" | awk '{print $1}')"
