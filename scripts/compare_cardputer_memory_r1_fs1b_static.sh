#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_ROOT="${OUT_ROOT:-${PROJECT_ROOT}/build/cardputer-memory-r1-fs1b-compare}"
STOCK_BUILD="${OUT_ROOT}/stock"
CANDIDATE_BUILD="${OUT_ROOT}/candidate"

rm -rf "${OUT_ROOT}"
mkdir -p "${OUT_ROOT}"

printf '%s\n' '=== MEMORY-R1 FS1B same-HEAD stock build ==='
BUILD_PATH="${STOCK_BUILD}" \
ARDUINO_BUILD_PATH="${STOCK_BUILD}/.arduino-build" \
  bash "${SCRIPT_DIR}/build.sh" "$@" | tee "${OUT_ROOT}/stock-build.log"

printf '%s\n' '=== MEMORY-R1 FS1B same-HEAD candidate build ==='
BUILD_PATH="${CANDIDATE_BUILD}" \
ARDUINO_BUILD_PATH="${CANDIDATE_BUILD}/.arduino-build" \
  bash "${SCRIPT_DIR}/build_cardputer_memory_r1_fs1b.sh" "$@" \
  | tee "${OUT_ROOT}/candidate-build.log"

find_one_elf() {
  local root="$1"
  local -a files=()
  mapfile -t files < <(find "$root" -type f -name '*.elf' -print | sort)
  if (( ${#files[@]} != 1 )); then
    printf 'Expected exactly one ELF under %s, found %d\n' "$root" "${#files[@]}" >&2
    printf '  %s\n' "${files[@]:-<none>}" >&2
    return 2
  fi
  printf '%s\n' "${files[0]}"
}

STOCK_ELF="$(find_one_elf "${STOCK_BUILD}/.arduino-build")"
CANDIDATE_ELF="$(find_one_elf "${CANDIDATE_BUILD}/.arduino-build")"

MEMORY_BASELINE_IMAGE_KIND=memory-r1-fs1b-stock \
  bash "${SCRIPT_DIR}/report_cardputer_memory_baseline.sh" "$STOCK_ELF" \
  | tee "${OUT_ROOT}/stock-memory.txt"
MEMORY_BASELINE_IMAGE_KIND=memory-r1-fs1b-candidate \
  bash "${SCRIPT_DIR}/report_cardputer_memory_baseline.sh" "$CANDIDATE_ELF" \
  | tee "${OUT_ROOT}/candidate-memory.txt"

python3 - \
  "${OUT_ROOT}/stock-memory.txt" \
  "${OUT_ROOT}/candidate-memory.txt" <<'PY'
from pathlib import Path
import re
import sys


def read_report(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8")
    match = re.search(
        r"MEMORY_BASELINE\s+.*?fixed=(\d+)\s+data=(\d+)\s+bss=(\d+)",
        text,
    )
    if not match:
        raise SystemExit(f"{path}: MEMORY_BASELINE machine summary not found")
    return {
        "fixed": int(match.group(1)),
        "data": int(match.group(2)),
        "bss": int(match.group(3)),
    }


stock = read_report(Path(sys.argv[1]))
candidate = read_report(Path(sys.argv[2]))
print(
    "FS1B ELF-MEASURED static DRAM: "
    f"stock={stock['fixed']} candidate={candidate['fixed']} "
    f"delta={stock['fixed'] - candidate['fixed']:+d} B recovery"
)
print(
    "FS1B ELF-MEASURED sections: "
    f"data {stock['data']} -> {candidate['data']}; "
    f"bss {stock['bss']} -> {candidate['bss']}"
)
PY
