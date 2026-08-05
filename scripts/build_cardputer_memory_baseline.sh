#!/usr/bin/env bash
set -euo pipefail

PROFILE="${1:-}"
case "${PROFILE}" in
  normal|midi-only) shift ;;
  *)
    echo "usage: build_cardputer_memory_baseline.sh <normal|midi-only> <product|runtime> [arduino-cli compile args...]" >&2
    exit 2
    ;;
esac

IMAGE_KIND="${1:-}"
case "${IMAGE_KIND}" in
  product|runtime) shift ;;
  *)
    echo "usage: build_cardputer_memory_baseline.sh <normal|midi-only> <product|runtime> [arduino-cli compile args...]" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SOURCE_COMMIT="$(git -C "${PROJECT_ROOT}" rev-parse HEAD 2>/dev/null || printf 'unknown')"
SOURCE_DIRTY="$(git -C "${PROJECT_ROOT}" status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
TEMP_ROOT="$(mktemp -d /tmp/grooveputer-memory-baseline.XXXXXX)"
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

if [[ "${IMAGE_KIND}" == "runtime" ]]; then
  python3 "${SOURCE_ROOT}/scripts/instrument_cardputer_memory_runtime.py" \
    "${SOURCE_ROOT}/GroovePuter.ino"
fi

case "${PROFILE}" in
  normal)
    export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    ;;
  midi-only)
    export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc"
    ;;
esac
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-memory-baseline-${PROFILE}-${IMAGE_KIND}}"

printf 'Memory baseline profile: %s\n' "${PROFILE}"
printf 'Memory baseline image: %s\n' "${IMAGE_KIND}"
printf 'Source commit: %s\n' "${SOURCE_COMMIT}"
printf 'Source dirty entries: %s\n' "${SOURCE_DIRTY}"
printf 'FQBN: %s\n' "${FQBN}"
printf 'Temporary source: %s\n' "${SOURCE_ROOT}"
printf 'Build output: %s\n' "${BUILD_PATH}"

bash "${SOURCE_ROOT}/scripts/build.sh" "$@"

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
MEMORY_BASELINE_IMAGE_KIND="${IMAGE_KIND}" \
  bash "${SOURCE_ROOT}/scripts/report_cardputer_memory_baseline.sh" "${ELF_PATH}"

if [[ "${IMAGE_KIND}" == "product" ]]; then
  bash "${SOURCE_ROOT}/scripts/report_cardputer_tinyusb_class_buffers.sh" \
    "${BUILD_PATH}"
fi

if [[ "${IMAGE_KIND}" == "runtime" ]]; then
  cat <<EOF

Flash this runtime-instrumented diagnostic image:
  BUILD_PATH=${BUILD_PATH} bash scripts/upload.sh /dev/ttyACM0
EOF
else
  cat <<'EOF'

This is an uninstrumented product-source ELF for exact static-section and
historical comparisons. Use the matching runtime image for hardware watermarks.
EOF
fi

cat <<'EOF'

The report above is non-gating. The product gate is provisionally restored to
191488 bytes while PR #70 derives profile-specific policy from hardware data.
EOF
