#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-h1"
mkdir -p "${BUILD}"

python3 "${ROOT}/tests/test_0_9_9_phrase_h1_source_contract.py"

SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/tests/test_0_9_9_phrase_h1_progression_what_source.cpp"
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

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${ROOT}" -O1 -g -fno-omit-frame-pointer -fsanitize=address \
  "${SOURCES[@]}" -o "${BUILD}/asan"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "${BUILD}/asan" > "${BUILD}/asan.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/asan.out"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${ROOT}" -O1 -g -fno-omit-frame-pointer -fsanitize=undefined \
  "${SOURCES[@]}" -o "${BUILD}/ubsan"
"${BUILD}/ubsan" > "${BUILD}/ubsan.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/ubsan.out"

cat "${BUILD}/gcc.out"
echo "0.9.9-PHRASE-H1 progression WHAT source gate: OK"
