#!/usr/bin/env bash
set -u
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/c9-midi-repair"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"
status=0

run_cpp() {
  local source="$1"
  local output="$2"
  if ! "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
      "${ROOT_DIR}/${source}" -o "${BUILD_DIR}/${output}"; then
    status=1
    return
  fi
  if ! "${BUILD_DIR}/${output}"; then
    status=1
  fi
}

run_cpp "tests/test_c9_tee_recovery.cpp" "test_c9_tee_recovery"
run_cpp "tests/test_c9_midi_input_dispatcher.cpp" "test_c9_midi_input_dispatcher"
if ! bash "${ROOT_DIR}/tests/test_c9_midi_user_surface.sh"; then
  status=1
fi

if [[ ${status} -ne 0 ]]; then
  echo "C9 MIDI repair contracts: FAIL"
  exit ${status}
fi

echo "C9 MIDI repair contracts: PASS"
