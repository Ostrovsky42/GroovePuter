#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e2a-mutation-producer"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e2a_source_contract.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer_evolution.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_vocabulary.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" \
    "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tests/test_0_9_9_e2a_mutation_producer.cpp" \
    -o "${BUILD_DIR}/test_0_9_9_e2a_${suffix}"
  "${BUILD_DIR}/test_0_9_9_e2a_${suffix}" | tee "${BUILD_DIR}/${suffix}.log"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
  grep '^E2A-CORPUS ' "${BUILD_DIR}/gcc.log" > "${BUILD_DIR}/gcc.corpus"
  grep '^E2A-CORPUS ' "${BUILD_DIR}/clang.log" > "${BUILD_DIR}/clang.corpus"
  diff -u "${BUILD_DIR}/gcc.corpus" "${BUILD_DIR}/clang.corpus"
fi

ASAN_OPTIONS=detect_leaks=0 \
  build_and_run "${CXX:-g++}" sanitize \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT_DIR}" -fstack-usage -c \
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer_evolution.cpp" \
  -o "${BUILD_DIR}/rhythm_realizer_evolution_e2a_stack.o"
python3 "${ROOT_DIR}/tests/test_0_9_9_e2a_stack_usage.py" \
  "${BUILD_DIR}/rhythm_realizer_evolution_e2a_stack.su"

# Frozen E2c contract and E1a owner/executor matrix remain authoritative.
bash "${ROOT_DIR}/tests/run_0_9_9_e2c_tests.sh"

# Keep fixed-seed phrase characterization green as ownership evolves.
bash "${ROOT_DIR}/tests/run_phrase_stage12_tests.sh"

printf '0.9.9-E2a canonical rhythm mutation producer: OK\n'
