#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/midi-io-contract-tests"
CXX="${CXX:-g++}"

if [[ "$#" -eq 0 ]]; then
  set -- all
fi

run_state() {
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "${ROOT_DIR}/tests/test_midi_io_state.cpp" \
    -o "${BUILD_DIR}/test_midi_io_state"
  "${BUILD_DIR}/test_midi_io_state"
}

run_output() {
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "${ROOT_DIR}/tests/test_midi_endpoint_dispatch.cpp" \
    -o "${BUILD_DIR}/test_midi_endpoint_dispatch"
  "${BUILD_DIR}/test_midi_endpoint_dispatch"
}

run_packet() {
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "${ROOT_DIR}/tests/test_nanokey2_packet_diagnostics.cpp" \
    -o "${BUILD_DIR}/test_nanokey2_packet_diagnostics"
  "${BUILD_DIR}/test_nanokey2_packet_diagnostics"
}

run_parser() {
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "${ROOT_DIR}/tests/test_midi_input_parser.cpp" \
    -o "${BUILD_DIR}/test_midi_input_parser"
  "${BUILD_DIR}/test_midi_input_parser"
}

mkdir -p "${BUILD_DIR}"
for group in "$@"; do
  case "${group}" in
    state) run_state ;;
    output) run_output ;;
    packet) run_packet ;;
    parser) run_parser ;;
    all) run_state; run_output; run_packet; run_parser ;;
    *)
      echo "unknown MIDI I/O contract group: ${group}" >&2
      exit 2
      ;;
  esac
done
