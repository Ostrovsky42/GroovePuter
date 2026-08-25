#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
GEN_DIR="${ROOT}/build/e3-listen-generated"
GEN_HEADER="${GEN_DIR}/e3_listen_fixture_generated.h"
BUILD_PATH="${BUILD_PATH:-${ROOT}/build/cardputer-adv-e3-listen}"
ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"

mkdir -p "${GEN_DIR}"

python3 "${ROOT}/tests/generate_0_9_9_e3_listen_fixture.py" \
  --output "${GEN_HEADER}"

STAGE_ROOT="$(mktemp -d /tmp/grooveputer-e3-listen-sketch.XXXXXX)"
cleanup() {
  rm -rf "${STAGE_ROOT}"
}
trap cleanup EXIT

STAGED="${STAGE_ROOT}/GroovePuter"
mkdir -p "${STAGED}"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'platform_sdl/build' \
  "${ROOT}/" "${STAGED}/"

python3 "${ROOT}/tests/prepare_0_9_9_e3_listen_sketch.py" \
  --root "${STAGED}" \
  --fixture "${GEN_HEADER}"

echo "=== Building disposable E3 LISTEN firmware ==="
echo "Tracked production source remains untouched; review seam exists only in ${STAGED}"
BUILD_PATH="${BUILD_PATH}" \
ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH}" \
  bash "${STAGED}/scripts/build.sh" "$@"

echo
echo "E3 LISTEN build ready:"
echo "  ${BUILD_PATH}/GroovePuter.ino.bin"
echo
echo "Flash with:"
echo "  BUILD_PATH=\"${BUILD_PATH}\" bash scripts/upload.sh /dev/ttyACM0"
