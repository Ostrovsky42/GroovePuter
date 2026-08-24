#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e3r-a-drop-displace-audit"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e3r_a_source_guard.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer_evolution.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_canonical_diff.cpp"
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
    "${ROOT_DIR}/tests/test_0_9_9_e3r_a_diff_contract.cpp" \
    -o "${BUILD_DIR}/test_0_9_9_e3r_a_${suffix}"
  "${BUILD_DIR}/test_0_9_9_e3r_a_${suffix}"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi

ASAN_OPTIONS=detect_leaks=0 \
  build_and_run "${CXX:-g++}" sanitize \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

# E2b reruns E2c, which reruns the E1a/Stage 6.1 owner/executor matrix.
bash "${ROOT_DIR}/tests/run_0_9_9_e2b_tests.sh"

# E2a owns the producer representation boundary and also reruns frozen E2c/E1a.
bash "${ROOT_DIR}/tests/run_0_9_9_e2a_tests.sh"

# Frozen V0R must remain byte-authoritative and fail closed for unsupported
# production operations; E3R-A does not regenerate or update its snapshots.
bash "${ROOT_DIR}/tests/run_0_9_9_v0r_variant_graph.sh"

printf '0.9.9-E3R-A DROP / DISPLACE execution contract audit: OK\n'
