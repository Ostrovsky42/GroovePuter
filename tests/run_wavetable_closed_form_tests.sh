#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "${OUT}"' EXIT
for opt in -O0 -O2; do
  g++ -std=c++17 "${opt}" -I"${ROOT_DIR}" "${ROOT_DIR}/tests/test_wavetable_closed_form.cpp" -o "${OUT}/t${opt}"
  "${OUT}/t${opt}"
done
# test_memory_r1_product_closure.py is RED at the base (instrumenter anchor drift), so the
# no-table guard is repeated here independently of it.
if grep -qE 'sawTable_|squareTable_' "${ROOT_DIR}/src/dsp/audio_wavetables.h" "${ROOT_DIR}/src/dsp/audio_wavetables.cpp"; then
  echo "saw/square table came back" >&2; exit 1
fi
echo "no saw/square table: OK"
