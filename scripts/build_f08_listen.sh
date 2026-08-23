#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
GEN_DIR="${ROOT}/build/f08-listen-generated"
GEN_HEADER="${GEN_DIR}/f08_listen_fixture_generated.h"
BUILD_PATH="${BUILD_PATH:-${ROOT}/build/cardputer-adv-f08-listen}"
ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"

mkdir -p "${GEN_DIR}"
python3 "${ROOT}/tests/generate_0_9_9_f08_listen_fixture.py" \
  --output "${GEN_HEADER}"

STAGE_ROOT="$(mktemp -d /tmp/grooveputer-f08-listen-sketch.XXXXXX)"
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

python3 "${ROOT}/tests/prepare_0_9_9_f08_listen_sketch.py" \
  --root "${STAGED}" \
  --fixture "${GEN_HEADER}"

echo "=== Building disposable F08 LISTEN firmware ==="
echo "Source F08 branch remains untouched; UI overlay exists only in ${STAGED}"
BUILD_PATH="${BUILD_PATH}" \
ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH}" \
  bash "${STAGED}/scripts/build.sh" "$@"

echo
echo "F08 LISTEN build ready:"
echo "  ${BUILD_PATH}/GroovePuter.ino.bin"
echo
echo "Flash with:"
echo "  BUILD_PATH=\"${BUILD_PATH}\" bash scripts/upload.sh /dev/ttyACM0"
