#!/usr/bin/env bash
set -euo pipefail

# Build the MEMORY-R1 FS1B runtime-diagnostic image from the exact source tree,
# using the same instrumentation pass as build_cardputer_memory_baseline.sh but
# linking the dynamic-buffer FatFs candidate instead of the stock archive.
# This image is for hardware watermarks/acceptance; it is not the production
# static-DRAM evidence image.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SOURCE_COMMIT="$(git -C "${PROJECT_ROOT}" rev-parse HEAD 2>/dev/null || printf 'unknown')"
SOURCE_DIRTY="$(git -C "${PROJECT_ROOT}" status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
TEMP_ROOT="$(mktemp -d /tmp/grooveputer-memory-r1-fs1b-runtime.XXXXXX)"
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

python3 "${SOURCE_ROOT}/scripts/instrument_cardputer_memory_runtime.py" \
  "${SOURCE_ROOT}"
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS:-} -DGROOVEPUTER_RUNTIME_DIAGNOSTICS=1"
export FQBN="${FQBN:-m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc}"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-memory-r1-fs1b-runtime}"
export ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"

printf 'MEMORY-R1 FS1B runtime diagnostic\n'
printf 'Source commit: %s\n' "${SOURCE_COMMIT}"
printf 'Source dirty entries: %s\n' "${SOURCE_DIRTY}"
printf 'FQBN: %s\n' "${FQBN}"
printf 'Temporary instrumented source: %s\n' "${SOURCE_ROOT}"
printf 'Build output: %s\n' "${BUILD_PATH}"

bash "${SOURCE_ROOT}/scripts/build_cardputer_memory_r1_fs1b.sh" "$@"

EXPECTED_ELF="${BUILD_PATH}/GroovePuter.ino.elf"
if [[ -f "${EXPECTED_ELF}" ]]; then
  ELF_PATH="${EXPECTED_ELF}"
else
  mapfile -t ELF_CANDIDATES < <(
    find "${BUILD_PATH}" -maxdepth 3 -type f -name '*.elf' -print | sort
  )
  if (( ${#ELF_CANDIDATES[@]} != 1 )); then
    printf 'Expected one ELF below %s, found %d\n' \
      "${BUILD_PATH}" "${#ELF_CANDIDATES[@]}" >&2
    printf '  %s\n' "${ELF_CANDIDATES[@]:-<none>}" >&2
    exit 2
  fi
  ELF_PATH="${ELF_CANDIDATES[0]}"
fi

ELF_SHA256="$(sha256sum "${ELF_PATH}" | awk '{print $1}')"
printf 'ELF path: %s\n' "${ELF_PATH}"
printf 'ELF sha256: %s\n' "${ELF_SHA256}"
MEMORY_BASELINE_IMAGE_KIND=memory-r1-fs1b-runtime \
  bash "${SOURCE_ROOT}/scripts/report_cardputer_memory_baseline.sh" "${ELF_PATH}"

cat <<EOF

Hardware-only next gate:
  flash this runtime diagnostic image and collect the MEMORY-R1 FS1B acceptance
  matrix. Do not book recovery from this build output alone.
EOF
