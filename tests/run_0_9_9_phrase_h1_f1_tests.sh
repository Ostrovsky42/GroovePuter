#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-h1-f1"
BASE="74456bcfec0fc74138ec0d8c652dde642c7e16b6"
mkdir -p "${BUILD}"

python3 "${ROOT}/tests/test_0_9_9_phrase_h1_f1_source_guard.py"

SOURCE_SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/tests/test_0_9_9_phrase_h1_f1_global_source.cpp"
)

build_and_capture() {
  local compiler="$1"
  local output="$2"
  local log="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -I"${ROOT}" "$@" "${SOURCE_SOURCES[@]}" -o "${output}"
  "${output}" > "${log}"
}

build_and_capture "${CXX:-g++}" "${BUILD}/gcc" "${BUILD}/gcc.out"
build_and_capture "${CXX:-g++}" "${BUILD}/gcc-repeat" "${BUILD}/gcc-repeat.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  build_and_capture clang++ "${BUILD}/clang" "${BUILD}/clang.out"
  diff -u "${BUILD}/gcc.out" "${BUILD}/clang.out"
else
  echo "clang++ is required for PHRASE-H1-F1" >&2
  exit 1
fi

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${ROOT}" -O1 -g -fno-omit-frame-pointer -fsanitize=address \
  "${SOURCE_SOURCES[@]}" -o "${BUILD}/asan"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "${BUILD}/asan" > "${BUILD}/asan.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/asan.out"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${ROOT}" -O1 -g -fno-omit-frame-pointer -fsanitize=undefined \
  "${SOURCE_SOURCES[@]}" -o "${BUILD}/ubsan"
"${BUILD}/ubsan" > "${BUILD}/ubsan.out"
diff -u "${BUILD}/gcc.out" "${BUILD}/ubsan.out"

# Compile the frozen H1 implementation from the exact predecessor SHA and
# compare the complete public finite-plan semantic corpus with this candidate.
BASE_ROOT="${BUILD}/frozen-h1"
mkdir -p "${BASE_ROOT}/src/generation/roles" "${BASE_ROOT}/src/generation/rhythm"
git -C "${ROOT}" show "${BASE}:src/generation/roles/chord_progression.h" \
  > "${BASE_ROOT}/src/generation/roles/chord_progression.h"
git -C "${ROOT}" show "${BASE}:src/generation/roles/chord_progression.cpp" \
  > "${BASE_ROOT}/src/generation/roles/chord_progression.cpp"
git -C "${ROOT}" show "${BASE}:src/generation/generation_context.h" \
  > "${BASE_ROOT}/src/generation/generation_context.h"
git -C "${ROOT}" show "${BASE}:src/generation/generation_context.cpp" \
  > "${BASE_ROOT}/src/generation/generation_context.cpp"
git -C "${ROOT}" show "${BASE}:src/generation/rhythm/rhythm_types.h" \
  > "${BASE_ROOT}/src/generation/rhythm/rhythm_types.h"

COMPAT_TEST="${ROOT}/tests/test_0_9_9_phrase_h1_f1_plan_compat.cpp"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${BASE_ROOT}" \
  "${BASE_ROOT}/src/generation/generation_context.cpp" \
  "${BASE_ROOT}/src/generation/roles/chord_progression.cpp" \
  "${COMPAT_TEST}" -o "${BUILD}/compat-base"
"${BUILD}/compat-base" > "${BUILD}/compat-base.out"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -I"${ROOT}" \
  "${ROOT}/src/generation/generation_context.cpp" \
  "${ROOT}/src/generation/roles/chord_progression.cpp" \
  "${COMPAT_TEST}" -o "${BUILD}/compat-candidate"
"${BUILD}/compat-candidate" > "${BUILD}/compat-candidate.out"
diff -u "${BUILD}/compat-base.out" "${BUILD}/compat-candidate.out"

cat "${BUILD}/gcc.out"
echo "PHRASE-H1-F1 finite ChordProgressionPlan base parity: PASS"
echo "0.9.9-PHRASE-H1-F1 global progression source gate: OK"