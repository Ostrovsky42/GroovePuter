#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
mkdir -p "${BUILD_DIR}"

mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

SOURCES=()
for source in "${COMMON_SOURCES[@]}"; do
  if [[ "${source}" == "/src/generation/migration/tonal_pattern_adapter.cpp" ]]; then
    continue
  fi
  SOURCES+=("${source}")
done

COMMON_FLAGS=(
  -std=c++17 -O2 -Wall -Wextra -Werror -Wvla
  -Wno-c++20-extensions -Wno-unused-variable -Wno-unused-but-set-variable
  -I"${ROOT}" -I"${ROOT}/platform_sdl"
  -include "${ROOT}/platform_sdl/arduino_compat.h"
)

compile_probe() {
  local compiler="$1"
  local suffix="$2"
  local adapter_obj="${BUILD_DIR}/g4_c0r3_tonal_pattern_adapter_${suffix}.o"
  local output="${BUILD_DIR}/gf2_gate_b_dump_c0r3_${suffix}"

  "${compiler}" "${COMMON_FLAGS[@]}" \
    -DadaptTonalPlanToSynthPattern=g4C0R3RealAdaptTonalPlanToSynthPattern \
    -c "${ROOT}/src/generation/migration/tonal_pattern_adapter.cpp" \
    -o "${adapter_obj}"

  "${compiler}" "${COMMON_FLAGS[@]}" \
    "${SOURCES[@]/#/${ROOT}}" \
    "${ROOT}/src/dsp/genre_manager.cpp" \
    "${ROOT}/scenes.cpp" \
    "${ROOT}/json_evented.cpp" \
    "${ROOT}/src/audio/pattern_paging.cpp" \
    "${ROOT}/tools/gf2/g4_c0r3_tonal_probe.cpp" \
    "${ROOT}/tools/gf2/g4_c0r3_dump.cpp" \
    "${adapter_obj}" \
    -o "${output}"
}

emit_fixture() {
  local binary="$1"
  local output="$2"
  local first=1
  : > "${output}"
  for profile in 0 9 20 27; do
    for identity in 7 19; do
      local tmp="${output}.${profile}.${identity}"
      "${binary}" --g4-c0r3-dump "${profile}" "${identity}" 0 23 P1 > "${tmp}"
      if [[ "${first}" -eq 1 ]]; then
        cat "${tmp}" >> "${output}"
        first=0
      else
        tail -n +2 "${tmp}" >> "${output}"
      fi
      rm -f "${tmp}"
    done
  done
}

compile_probe "${CXX:-g++}" gcc
emit_fixture \
  "${BUILD_DIR}/gf2_gate_b_dump_c0r3_gcc" \
  "${BUILD_DIR}/g4-c0r3-probe-gcc.tsv"

echo "G4-C0R3 probe GCC: BUILD+RUN PASS"

if command -v clang++ >/dev/null 2>&1; then
  compile_probe clang++ clang
  emit_fixture \
    "${BUILD_DIR}/gf2_gate_b_dump_c0r3_clang" \
    "${BUILD_DIR}/g4-c0r3-probe-clang.tsv"
  cmp \
    "${BUILD_DIR}/g4-c0r3-probe-gcc.tsv" \
    "${BUILD_DIR}/g4-c0r3-probe-clang.tsv"
  echo "G4-C0R3 probe GCC/Clang: BYTE-IDENTICAL"
fi
