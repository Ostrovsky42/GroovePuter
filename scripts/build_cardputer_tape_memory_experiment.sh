#!/usr/bin/env bash
set -euo pipefail

VARIANT="${1:-}"
case "${VARIANT}" in
  A|B|C) shift ;;
  *)
    echo "usage: build_cardputer_tape_memory_experiment.sh <A|B|C> [normal|midi-only] [arduino-cli compile args...]" >&2
    exit 2
    ;;
esac

PROFILE="${1:-normal}"
case "${PROFILE}" in
  normal|midi-only) shift ;;
  *)
    echo "usage: build_cardputer_tape_memory_experiment.sh <A|B|C> [normal|midi-only] [arduino-cli compile args...]" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SOURCE_COMMIT="$(git -C "${PROJECT_ROOT}" rev-parse HEAD 2>/dev/null || printf 'unknown')"
SOURCE_DIRTY="$(git -C "${PROJECT_ROOT}" status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
TEMP_ROOT="$(mktemp -d /tmp/grooveputer-tape-memory.XXXXXX)"
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

python3 "${SOURCE_ROOT}/scripts/instrument_tape_memory_experiment.py" \
  "${SOURCE_ROOT}" "${VARIANT}"
python3 "${SOURCE_ROOT}/scripts/instrument_cardputer_memory_runtime.py" \
  "${SOURCE_ROOT}"

case "${PROFILE}" in
  normal)
    export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    ;;
  midi-only)
    export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc"
    ;;
esac

export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/tape-memory-${VARIANT}-${PROFILE}}"

printf 'Tape memory variant: %s\n' "${VARIANT}"
printf 'Profile: %s\n' "${PROFILE}"
printf 'Source commit: %s\n' "${SOURCE_COMMIT}"
printf 'Source dirty entries: %s\n' "${SOURCE_DIRTY}"
printf 'FQBN: %s\n' "${FQBN}"
printf 'Temporary source: %s\n' "${SOURCE_ROOT}"
printf 'Build output: %s\n' "${BUILD_PATH}"

BUILD_PATH="${BUILD_PATH}" bash "${SOURCE_ROOT}/scripts/build.sh" "$@"

EXPECTED_ELF="${BUILD_PATH}/GroovePuter.ino.elf"
if [[ -f "${EXPECTED_ELF}" ]]; then
  ELF_PATH="${EXPECTED_ELF}"
else
  mapfile -t ELF_CANDIDATES < <(
    find "${BUILD_PATH}" -maxdepth 2 -type f -name '*.elf' -print | sort
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
MEMORY_BASELINE_IMAGE_KIND="runtime" \
  bash "${SOURCE_ROOT}/scripts/report_cardputer_memory_baseline.sh" "${ELF_PATH}"

cat <<EOF

Flash variant ${VARIANT} on the same Cardputer ADV:
  BUILD_PATH=${BUILD_PATH} bash scripts/upload.sh /dev/ttyACM0

Serial checkpoints to capture:
  [tape-exp-after-static-construction]
  [tape-exp-after-audio-task]
  [tape-exp-after-sd]
  [tape-exp-after-miniacid-init]
  [tape-exp-after-ui]
  [MEM-BASE] phase=runtime-start
  [MEM-BASE] phase=periodic
EOF
