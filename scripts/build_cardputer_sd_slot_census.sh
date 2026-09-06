#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# R1-A: what one FatFs file slot costs, measured on the real card without an
# SDK fork and without changing production mount arguments.
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS:-} -DGROOVEPUTER_SD_SLOT_CENSUS"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-sd-slot-census}"
bash "${SCRIPT_DIR}/build.sh" "$@"
