#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TEMP_ROOT="$(mktemp -d /tmp/grooveputer-fs1d.XXXXXX)"
SOURCE_ROOT="${TEMP_ROOT}/GroovePuter"

cleanup() {
  rm -rf "${TEMP_ROOT}"
}
trap cleanup EXIT

mkdir -p "${SOURCE_ROOT}"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'platform_sdl/build' \
  "${PROJECT_ROOT}/" "${SOURCE_ROOT}/"

python3 "${SOURCE_ROOT}/tests/fs1d/instrument_pending_residency.py" \
  "${SOURCE_ROOT}"

export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/fs1d-pending-residency}"
export ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"

printf 'FS1D source checkout: %s\n' "$(git -C "${PROJECT_ROOT}" rev-parse HEAD)"
printf 'FS1D temporary source: %s\n' "${SOURCE_ROOT}"
printf 'FS1D build output: %s\n' "${BUILD_PATH}"

# Reuse the accepted FS1B Cardputer/FatFs build path. The only source mutation
# happens in SOURCE_ROOT, which is deleted after the diagnostic image is built.
bash "${SOURCE_ROOT}/scripts/build_cardputer_dynbuffers.sh" "$@"

printf '%s\n' '=== FS1D read-only pending-residency diagnostic build PASS ==='
