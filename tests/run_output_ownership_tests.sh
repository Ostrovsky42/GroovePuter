#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/output-ownership"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -I"${ROOT_DIR}"
)

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_output_ownership.cpp" \
  -o "${BUILD_DIR}/test_output_ownership"

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_output_ownership_queues.cpp" \
  -o "${BUILD_DIR}/test_output_ownership_queues"

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_output_scene_persistence.cpp" \
  "${ROOT_DIR}/src/eye_pair_sync/eye_output_mode.cpp" \
  -o "${BUILD_DIR}/test_output_scene_persistence"

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_eye_output_mode.cpp" \
  "${ROOT_DIR}/src/eye_pair_sync/eye_output_mode.cpp" \
  -o "${BUILD_DIR}/test_eye_output_mode"

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_eye_udp_integration.cpp" \
  "${ROOT_DIR}/src/eye_pair_sync/eye_output_mode.cpp" \
  -o "${BUILD_DIR}/test_eye_udp_integration"

"${BUILD_DIR}/test_output_ownership"
"${BUILD_DIR}/test_output_ownership_queues"
"${BUILD_DIR}/test_output_scene_persistence"
"${BUILD_DIR}/test_eye_output_mode"
"${BUILD_DIR}/test_eye_udp_integration"
python3 "${ROOT_DIR}/tests/test_output_ownership_source_contract.py"
python3 "${ROOT_DIR}/tests/test_gvep_r01_source_contract.py"

echo "0.9.6 output ownership contract gate: PASS"
