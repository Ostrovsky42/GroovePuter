#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# Acceptance item G: prove five simultaneous logical file handles still open
# and are writable once FatFs buffers are allocated at f_open() rather than
# reserved at mount.
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS:-} -DGROOVEPUTER_SD_HANDLE_CENSUS"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-sd-handle-census}"
bash "${SCRIPT_DIR}/build.sh" "$@"
