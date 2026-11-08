#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/material-slot-identity"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -Wno-c++20-extensions -I. -Iplatform_sdl \
  -include platform_sdl/arduino_compat.h \
  tests/test_material_slot_identity.cpp -o "${BUILD_DIR}/test"
"${BUILD_DIR}/test"
printf '%s\n' 'M1 material slot identity gate: PASS'
