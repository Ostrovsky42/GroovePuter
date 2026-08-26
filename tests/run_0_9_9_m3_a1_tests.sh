#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/m3-a1"
mkdir -p "${BUILD}"

python3 "${ROOT}/tests/test_0_9_9_m3_a1_source_contract.py"

SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/src/generation/tonal/tonal_projector.cpp"
  "${ROOT}/src/generation/tonal/tonal_materializer.cpp"
  "${ROOT}/tests/test_0_9_9_m3_a1_harmonic_crossing.cpp"
)

build_and_capture() {
  local compiler="$1"
  local output="$2"
  local log="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -I"${ROOT}" "$@" "${SOURCES[@]}" -o "${output}"
  "${output}" > "${log}"
}

build_and_capture "${CXX:-g++}" "${BUILD}/gcc" "${BUILD}/gcc.out"
build_and_capture "${CXX:-g++}" "${BUILD}/gcc-repeat" "${BUILD}/gcc-repeat.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  build_and_capture clang++ "${BUILD}/clang" "${BUILD}/clang.out"
  diff -u "${BUILD}/gcc.out" "${BUILD}/clang.out"
fi

build_and_capture "${CXX:-g++}" "${BUILD}/sanitize" "${BUILD}/sanitize.out" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "${BUILD}/sanitize" > "${BUILD}/sanitize-runtime.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/sanitize-runtime.out"

cat "${BUILD}/gcc.out"

# Preserve the hardware-accepted M1 phrase/address contract and the existing
# local tonal materializer behavior while characterizing the missing crossing carrier.
bash "${ROOT}/tests/run_0_9_9_m1_p1_tests.sh"
bash "${ROOT}/tests/run_tonal_materializer_tests.sh"

echo "0.9.9-M3-A1 harmonic crossing audit gate: OK"
