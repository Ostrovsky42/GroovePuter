#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/stage6-1"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage6_1_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/bar_evolution.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" \
    "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tests/test_rhythm_stage6_1_hardening.cpp" \
    -o "${BUILD_DIR}/test_rhythm_stage6_1_${suffix}"
  "${BUILD_DIR}/test_rhythm_stage6_1_${suffix}"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi

build_and_run "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

# GCC static stack-usage output is a regression guard only. The actual
# ESP32-S3 task high-water mark remains a hardware gate before production
# BarEvolution wiring.
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT_DIR}" -fstack-usage -c \
  "${ROOT_DIR}/src/generation/rhythm/bar_evolution.cpp" \
  -o "${BUILD_DIR}/bar_evolution_stack.o"
python3 "${ROOT_DIR}/tests/test_rhythm_stage6_1_stack_usage.py" \
  "${BUILD_DIR}/bar_evolution_stack.su"

printf 'Groove Vocabulary Stage 6.1 hardening host matrix: OK\n'
