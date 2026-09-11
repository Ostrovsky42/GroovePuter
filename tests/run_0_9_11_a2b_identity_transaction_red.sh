#!/usr/bin/env bash
set -euo pipefail

CXX="${CXX:-g++}"
BUILD_DIR="${TMPDIR:-/tmp}/grooveputer-a2b-identity-transaction"
BINARY="${BUILD_DIR}/test_a2b_identity_transaction"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++20 \
  -Wall -Wextra -Werror \
  -I. \
  -Iplatform_sdl \
  tests/test_0_9_11_a2b_identity_transaction.cpp \
  -o "${BINARY}"

"${BINARY}"
