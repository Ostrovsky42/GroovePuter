#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e2b-canonical-diff-budget"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e2b_source_contract.py"

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
    "${ROOT_DIR}/tests/test_0_9_9_e2b_canonical_diff_budget.cpp" \
    -o "${BUILD_DIR}/test_0_9_9_e2b_${suffix}"
  "${BUILD_DIR}/test_0_9_9_e2b_${suffix}"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi

build_and_run "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

# Guard the bounded stack implementation. This is a host compiler proxy, not
# ESP32-S3 runtime high-water data; fixed-DRAM firmware CI remains authoritative.
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT_DIR}" -fstack-usage -c \
  "${ROOT_DIR}/src/generation/rhythm/rhythm_canonical_diff.cpp" \
  -o "${BUILD_DIR}/rhythm_canonical_diff_stack.o"
python3 - "${BUILD_DIR}/rhythm_canonical_diff_stack.su" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
maximum = 0
for line in path.read_text(encoding="utf-8").splitlines():
    match = re.search(r"\t(\d+)\t", line)
    if match:
        maximum = max(maximum, int(match.group(1)))
if maximum > 1024:
    raise SystemExit(f"E2b stack frame regression: {maximum} bytes > 1024")
print(f"E2B bounded host stack proxy: PASS (max={maximum} bytes)")
PY

# Re-run the frozen E2c contract; it in turn reruns the exact E1a/Stage 6.1
# host matrix, including GCC/Clang/sanitizer and E1a ownership regressions.
bash "${ROOT_DIR}/tests/run_0_9_9_e2c_tests.sh"

printf '0.9.9-E2b canonical rhythm diff / budget: OK\n'
