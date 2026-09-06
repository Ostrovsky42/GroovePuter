#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SDK_SOURCE="${M5STACK_ESP32S3_SDK:-$HOME/.arduino15/packages/m5stack/tools/esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3}"
FATFS_OUT="${FATFS_OUT:-/tmp/fatfs-build}"
SDK_OVERLAY="${SDK_OVERLAY:-/tmp/grooveputer-sdk-dynbuffers}"
BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-dynbuffers}"
ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"

if [[ ! -d "${SDK_SOURCE}" ]]; then
  echo "M5Stack ESP32-S3 SDK not found: ${SDK_SOURCE}" >&2
  exit 1
fi

# Reproduce the accepted FS1-B archive first. This does not modify SDK_SOURCE.
OUT="${FATFS_OUT}" M5STACK_ESP32S3_SDK="${SDK_SOURCE}" \
  bash "${SCRIPT_DIR}/build_fatfs_dynbuffers_candidate.sh"

# Build a cheap, disposable SDK overlay: all top-level SDK entries are symlinked
# read-only except lib/, which is a real directory containing symlinks to stock
# archives with only libfatfs.a replaced by the accepted candidate. This makes
# the normal platform -L{compiler.sdk.path}/lib resolve one coherent archive and
# avoids build.extra_libs / --start-group archive mixing.
rm -rf "${SDK_OVERLAY}"
mkdir -p "${SDK_OVERLAY}/lib"
for entry in "${SDK_SOURCE}"/*; do
  base="$(basename "${entry}")"
  [[ "${base}" == "lib" ]] && continue
  ln -s "${entry}" "${SDK_OVERLAY}/${base}"
done
for archive in "${SDK_SOURCE}/lib"/*; do
  base="$(basename "${archive}")"
  [[ "${base}" == "libfatfs.a" ]] && continue
  ln -s "${archive}" "${SDK_OVERLAY}/lib/${base}"
done
ln -s "${FATFS_OUT}/libfatfs.a" "${SDK_OVERLAY}/lib/libfatfs.a"

export BUILD_PATH ARDUINO_BUILD_PATH
bash "${SCRIPT_DIR}/build.sh" \
  --build-property "compiler.sdk.path=${SDK_OVERLAY}" \
  "$@"

MAP="${ARDUINO_BUILD_PATH}/GroovePuter.ino.map"
if [[ ! -f "${MAP}" ]]; then
  echo "Link map not found: ${MAP}" >&2
  exit 1
fi

CANDIDATE_PATH="${SDK_OVERLAY}/lib/libfatfs.a"
STOCK_PATH="${SDK_SOURCE}/lib/libfatfs.a"
if ! grep -Fq "${CANDIDATE_PATH}" "${MAP}"; then
  echo "FS1 link proof FAILED: candidate libfatfs.a is absent from ${MAP}" >&2
  exit 1
fi
if grep -Fq "${STOCK_PATH}" "${MAP}"; then
  echo "FS1 link proof FAILED: stock libfatfs.a also appears in ${MAP}" >&2
  exit 1
fi

echo "=== FS1 dynamic-FatFs Cardputer build PASS ==="
echo "candidate: ${FATFS_OUT}/libfatfs.a"
echo "sdk overlay: ${SDK_OVERLAY}"
echo "build: ${BUILD_PATH}"
echo "map proof: candidate only"
