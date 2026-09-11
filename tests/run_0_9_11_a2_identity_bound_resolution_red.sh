#!/usr/bin/env bash
set -euo pipefail

CXX="${CXX:-g++}"
BUILD_DIR="${TMPDIR:-/tmp}/grooveputer-a2-identity-bound-resolution"
BINARY="${BUILD_DIR}/test_a2_identity_bound_resolution"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I. \
  -Iplatform_sdl \
  tests/test_0_9_11_a2_identity_bound_resolution.cpp \
  -o "${BINARY}"

"${BINARY}"
