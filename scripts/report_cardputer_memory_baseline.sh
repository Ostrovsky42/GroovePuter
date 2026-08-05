#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:?usage: report_cardputer_memory_baseline.sh <firmware.elf>}"
IMAGE_KIND="${MEMORY_BASELINE_IMAGE_KIND:-unspecified}"
PROVISIONAL_GATE_BYTES="${PROVISIONAL_GATE_BYTES:-191488}"
UNSUPPORTED_GATE_BYTES="${UNSUPPORTED_GATE_BYTES:-122880}"

if [[ ! -f "${ELF_PATH}" ]]; then
  echo "Firmware ELF not found: ${ELF_PATH}" >&2
  exit 2
fi

discover_arduino_tool() {
  local tool_name="$1"
  local override="${2:-}"
  local path_candidate=""
  local package_root="${ARDUINO_PACKAGES_ROOT:-${HOME}/.arduino15/packages}"
  local -a candidates=()

  if [[ -n "${override}" ]]; then
    if [[ -x "${override}" ]]; then
      printf '%s\n' "${override}"
      return 0
    fi
    echo "Configured ${tool_name} is not executable: ${override}" >&2
    return 2
  fi

  path_candidate="$(command -v "${tool_name}" 2>/dev/null || true)"
  if [[ -n "${path_candidate}" && -x "${path_candidate}" ]]; then
    printf '%s\n' "${path_candidate}"
    return 0
  fi

  if [[ -d "${package_root}" ]]; then
    mapfile -t candidates < <(
      find "${package_root}" -type f -name "${tool_name}" -print 2>/dev/null \
        | sort -V || true
    )
  fi
  if (( ${#candidates[@]} == 0 )); then
    echo "${tool_name} was not found in PATH or ${package_root}" >&2
    return 2
  fi

  path_candidate="${candidates[$((${#candidates[@]} - 1))]}"
  if [[ ! -x "${path_candidate}" ]]; then
    echo "Discovered ${tool_name} is not executable: ${path_candidate}" >&2
    return 2
  fi
  printf '%s\n' "${path_candidate}"
}

SIZE_TOOL="$(discover_arduino_tool xtensa-esp32s3-elf-size "${SIZE_TOOL:-}")"
OBJDUMP_TOOL="$(discover_arduino_tool xtensa-esp32s3-elf-objdump "${OBJDUMP_TOOL:-}")"

read -r DRAM_DATA DRAM_BSS < <(
  "${SIZE_TOOL}" -A "${ELF_PATH}" | awk '
    $1 == ".dram0.data" { data = $2 }
    $1 == ".dram0.bss" { bss = $2 }
    END { print data + 0, bss + 0 }
  '
)
DRAM_TOTAL=$((DRAM_DATA + DRAM_BSS))
PROVISIONAL_HEADROOM=$((PROVISIONAL_GATE_BYTES - DRAM_TOTAL))
UNSUPPORTED_HEADROOM=$((UNSUPPORTED_GATE_BYTES - DRAM_TOTAL))

printf '%s\n' "=== Cardputer memory baseline ==="
printf 'image kind: %s\n' "${IMAGE_KIND}"
printf 'ELF: %s\n' "${ELF_PATH}"
printf '.dram0.data: %d bytes\n' "${DRAM_DATA}"
printf '.dram0.bss:  %d bytes\n' "${DRAM_BSS}"
printf 'fixed DRAM:  %d bytes\n' "${DRAM_TOTAL}"
printf 'provisional gate: %d bytes (headroom %+d; not safety-derived)\n' \
  "${PROVISIONAL_GATE_BYTES}" "${PROVISIONAL_HEADROOM}"
printf 'unsupported 122880 reference: %d bytes (headroom %+d)\n' \
  "${UNSUPPORTED_GATE_BYTES}" "${UNSUPPORTED_HEADROOM}"

DRAM_SYMBOLS="$(mktemp)"
cleanup() {
  rm -f "${DRAM_SYMBOLS}"
}
trap cleanup EXIT

# objdump carries the actual section name for every symbol. This avoids
# treating all nm B/b symbols as .dram0.bss when an ELF may also contain RTC or
# other BSS-like sections. Output is sorted by size for stable reports.
"${OBJDUMP_TOOL}" -t "${ELF_PATH}" | python3 -c '
import sys

records = []
for raw in sys.stdin:
    fields = raw.split()
    if not fields:
        continue
    section_index = next(
        (i for i, value in enumerate(fields)
         if value in {".dram0.data", ".dram0.bss"}),
        None,
    )
    if section_index is None or section_index + 2 >= len(fields):
        continue
    try:
        address = int(fields[0], 16)
        size = int(fields[section_index + 1], 16)
    except ValueError:
        continue
    if size <= 0:
        continue
    section = fields[section_index]
    name = " ".join(fields[section_index + 2:])
    if not name:
        continue
    records.append((size, address, section, name))

for size, address, section, name in sorted(records):
    print(f"0x{address:x} {size} {section} {name}")
' > "${DRAM_SYMBOLS}"

print_matches() {
  local label="$1"
  shift
  local pattern
  local found=0
  local total=0
  printf '\n%s:\n' "${label}"
  for pattern in "$@"; do
    while read -r address size section name; do
      [[ -n "${name:-}" ]] || continue
      if [[ "${section}" == ".dram0.bss" && "${name}" == *"${pattern}"* ]]; then
        printf '  %8d  %-11s  %s\n' "${size}" "${section}" "${name}"
        total=$((total + size))
        found=1
      fi
    done < "${DRAM_SYMBOLS}"
  done
  if (( found == 0 )); then
    echo "  not found"
  else
    printf '  subtotal: %d bytes\n' "${total}"
  fi
}

print_matches "Scene transaction scratch" "s_tempLoadScene"
print_matches "Active Scene" "g_mainScene"
print_matches "Wavetable static arrays" \
  "sineTable_" "squareTable_" "triangleTable_" "sawTable_"
print_matches "SMF player runtime" "g_smfPlayer"
print_matches "MiniAcid engine" "g_miniAcidInstance"

read -r BSS_SYMBOL_RAW BSS_SYMBOL_COVERAGE BSS_ALIAS_OVERLAP CANDIDATE_BSS_COVERAGE < <(
  python3 - "${DRAM_SYMBOLS}" <<'PY'
from pathlib import Path
import sys

candidate_parts = (
    "s_tempLoadScene",
    "g_mainScene",
    "sineTable_",
    "squareTable_",
    "triangleTable_",
    "sawTable_",
    "g_smfPlayer",
    "g_miniAcidInstance",
)

records = []
for raw in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    fields = raw.split(maxsplit=3)
    if len(fields) != 4 or fields[2] != ".dram0.bss":
        continue
    address = int(fields[0], 0)
    size = int(fields[1], 10)
    records.append((address, address + size, fields[3]))


def coverage(intervals):
    merged = []
    for start, end in sorted(intervals):
        if not merged or start > merged[-1][1]:
            merged.append([start, end])
        elif end > merged[-1][1]:
            merged[-1][1] = end
    return sum(end - start for start, end in merged)

raw_total = sum(end - start for start, end, _ in records)
all_coverage = coverage((start, end) for start, end, _ in records)
candidate_coverage = coverage(
    (start, end)
    for start, end, name in records
    if any(part in name for part in candidate_parts)
)
print(raw_total, all_coverage, raw_total - all_coverage, candidate_coverage)
PY
)

BSS_SECTION_UNCOVERED=$((DRAM_BSS - BSS_SYMBOL_COVERAGE))
CANDIDATE_OUTSIDE=$((DRAM_BSS - CANDIDATE_BSS_COVERAGE))
if (( BSS_SECTION_UNCOVERED < 0 || CANDIDATE_OUTSIDE < 0 )); then
  printf 'Symbol coverage exceeds .dram0.bss: coverage=%d candidate=%d section=%d\n' \
    "${BSS_SYMBOL_COVERAGE}" "${CANDIDATE_BSS_COVERAGE}" "${DRAM_BSS}" >&2
  exit 2
fi

printf '\nBSS attribution summary:\n'
printf '  named candidate shortlist:   %d bytes\n' "${CANDIDATE_BSS_COVERAGE}"
printf '  outside candidate shortlist: %d bytes\n' "${CANDIDATE_OUTSIDE}"
printf '  all symbol interval coverage: %d bytes\n' "${BSS_SYMBOL_COVERAGE}"
printf '  section bytes not covered:    %d bytes\n' "${BSS_SECTION_UNCOVERED}"
printf '  raw symbol-size overlap:       %d bytes\n' "${BSS_ALIAS_OVERLAP}"
printf '%s\n' \
  '  Note: "outside candidate shortlist" is attributed by the full inventory below; it is not assumed free or waste.'

printf '\nFull .dram0.bss symbol inventory (ascending by size):\n'
awk '$3 == ".dram0.bss" {
  name = $4
  for (i = 5; i <= NF; ++i) name = name " " $i
  printf "  %8d  %-11s  %-12s  %s\n", $2 + 0, $3, $1, name
}' "${DRAM_SYMBOLS}"

printf '\nLargest exact .dram0.data/.dram0.bss symbols:\n'
tail -n 40 "${DRAM_SYMBOLS}" | awk '{
  name = $4
  for (i = 5; i <= NF; ++i) name = name " " $i
  printf "  %8d  %-11s  %-12s  %s\n", $2 + 0, $3, $1, name
}'

printf '\nMachine summary:\n'
printf 'MEMORY_BASELINE kind=%s fixed=%d data=%d bss=%d bss_symbol_raw=%d bss_symbol_coverage=%d bss_section_uncovered=%d bss_alias_overlap=%d candidate_bss=%d candidate_outside=%d provisional_gate=%d provisional_headroom=%+d provisional_status=exception_unvalidated unsupported_gate=%d unsupported_headroom=%+d\n' \
  "${IMAGE_KIND}" "${DRAM_TOTAL}" "${DRAM_DATA}" "${DRAM_BSS}" \
  "${BSS_SYMBOL_RAW}" "${BSS_SYMBOL_COVERAGE}" \
  "${BSS_SECTION_UNCOVERED}" "${BSS_ALIAS_OVERLAP}" \
  "${CANDIDATE_BSS_COVERAGE}" "${CANDIDATE_OUTSIDE}" \
  "${PROVISIONAL_GATE_BYTES}" "${PROVISIONAL_HEADROOM}" \
  "${UNSUPPORTED_GATE_BYTES}" "${UNSUPPORTED_HEADROOM}"

# This script is deliberately non-gating. The mandatory gate is restored only
# under an explicit provisional exception; it is not a safety-derived ceiling.
# Profile-specific runtime evidence and a reserve calculation are still needed.
exit 0
