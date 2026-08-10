#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-gvep-r0}"
export ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"

exec "${SCRIPT_DIR}/build.sh" \
  --build-property "compiler.cpp.extra_flags=-DGROOVEPUTER_GVEP_R0=1" \
  "$@"
