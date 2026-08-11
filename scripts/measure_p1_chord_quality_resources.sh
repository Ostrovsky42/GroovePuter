#!/usr/bin/env bash
set -euo pipefail

PROFILE="${1:-normal}"
case "${PROFILE}" in
  normal|midi-only) ;;
  *) echo "usage: measure_p1_chord_quality_resources.sh <normal|midi-only>" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE_BUILD="${ROOT}/build/p1-chord-quality-${PROFILE}-default"
PROBE_BUILD="${ROOT}/build/p1-chord-quality-${PROFILE}-retained"
LOOP_STACK="${ARDUINO_LOOP_STACK_SIZE:-12288}"

rm -rf "${BASE_BUILD}" "${PROBE_BUILD}"

COMMON_CPP_FLAGS="-DARDUINO_LOOP_STACK_SIZE=${LOOP_STACK} -fstack-usage"

printf '=== P1 default product build (%s) ===\n' "${PROFILE}"
BUILD_PATH="${BASE_BUILD}" \
  bash "${ROOT}/scripts/build_cardputer_memory_baseline.sh" \
    "${PROFILE}" product --warnings all \
    --build-property "compiler.cpp.extra_flags=${COMMON_CPP_FLAGS}"

printf '\n=== P1 retained-probe product build (%s) ===\n' "${PROFILE}"
BUILD_PATH="${PROBE_BUILD}" \
  bash "${ROOT}/scripts/build_cardputer_memory_baseline.sh" \
    "${PROFILE}" product --warnings all \
    --build-property "compiler.cpp.extra_flags=${COMMON_CPP_FLAGS}" \
    --build-property "compiler.c.elf.extra_flags=-ugrooveputerP1ChordQualityProbe"

discover_tool() {
  local name="$1"
  local root="${ARDUINO_PACKAGES_ROOT:-${HOME}/.arduino15/packages}"
  local found=""
  found="$(command -v "${name}" 2>/dev/null || true)"
  if [[ -n "${found}" ]]; then printf '%s\n' "${found}"; return 0; fi
  found="$(find "${root}" -type f -name "${name}" -perm -u+x -print 2>/dev/null | sort -V | tail -n1)"
  test -n "${found}"
  printf '%s\n' "${found}"
}

SIZE_TOOL="$(discover_tool xtensa-esp32s3-elf-size)"
NM_TOOL="$(discover_tool xtensa-esp32s3-elf-nm)"

base_elf="${BASE_BUILD}/GroovePuter.ino.elf"
probe_elf="${PROBE_BUILD}/GroovePuter.ino.elf"
test -f "${base_elf}"
test -f "${probe_elf}"

# Feature-OFF / feasibility-only proof: the ordinary product link must not
# retain either the probe or the projector. The forced link must retain both.
base_nm="${BASE_BUILD}/p1-symbols.txt"
probe_nm="${PROBE_BUILD}/p1-symbols.txt"
"${NM_TOOL}" -C "${base_elf}" > "${base_nm}"
"${NM_TOOL}" -C "${probe_elf}" > "${probe_nm}"
if grep -qE 'grooveputerP1ChordQualityProbe|projectChordQualityPitchSet' "${base_nm}"; then
  echo "P1 chord-quality projector leaked into the default product link" >&2
  exit 1
fi
grep -q 'grooveputerP1ChordQualityProbe' "${probe_nm}"
grep -q 'projectChordQualityPitchSet' "${probe_nm}"

read_size() {
  local elf="$1"
  "${SIZE_TOOL}" "${elf}" | awk 'NR == 2 {print $1, $2, $3}'
}

read_fixed_dram() {
  local elf="$1"
  "${SIZE_TOOL}" -A "${elf}" | awk '
    $1 == ".dram0.data" { data = $2 }
    $1 == ".dram0.bss" { bss = $2 }
    END { print data + bss }
  '
}

read -r base_text base_data base_bss < <(read_size "${base_elf}")
read -r probe_text probe_data probe_bss < <(read_size "${probe_elf}")
base_fixed="$(read_fixed_dram "${base_elf}")"
probe_fixed="$(read_fixed_dram "${probe_elf}")"

text_delta=$((probe_text - base_text))
data_delta=$((probe_data - base_data))
bss_delta=$((probe_bss - base_bss))
fixed_delta=$((probe_fixed - base_fixed))
load_delta=$((text_delta + data_delta))

mapfile -t object_candidates < <(
  find "${PROBE_BUILD}/.arduino-build" -type f \
    -name '*chord_quality_projector.cpp.o' -print | sort
)
if (( ${#object_candidates[@]} != 1 )); then
  printf 'Expected one chord_quality_projector object, found %d\n' \
    "${#object_candidates[@]}" >&2
  printf '  %s\n' "${object_candidates[@]:-<none>}" >&2
  exit 2
fi
object="${object_candidates[0]}"
read -r object_text object_data object_bss < <(read_size "${object}")

mapfile -t stack_candidates < <(
  find "${PROBE_BUILD}/.arduino-build" -type f \
    -name '*chord_quality_projector*.su' -print | sort
)
if (( ${#stack_candidates[@]} == 0 )); then
  echo "No Xtensa stack-usage file found for chord_quality_projector" >&2
  exit 2
fi
stack_bytes="$(awk -F '\t' '/projectChordQualityPitchSet/ {print $2; exit}' "${stack_candidates[@]}")"
test -n "${stack_bytes}"

# Feasibility gates are deliberately conservative and apply only to this
# isolated projector, not to a future polyphonic physical voice implementation.
test "${fixed_delta}" -le 16
test "${bss_delta}" -le 16
test "${load_delta}" -le 2048
test "${stack_bytes}" -le 256

printf '\n=== P1 ESP32-S3 resource delta (%s) ===\n' "${PROFILE}"
printf 'default link retains P1 projector: NO\n'
printf 'forced probe link retains P1 projector: YES\n'
printf 'default ELF text/data/bss: %d / %d / %d B\n' \
  "${base_text}" "${base_data}" "${base_bss}"
printf 'retained ELF text/data/bss: %d / %d / %d B\n' \
  "${probe_text}" "${probe_data}" "${probe_bss}"
printf 'linked text delta: %+d B\n' "${text_delta}"
printf 'linked data delta: %+d B\n' "${data_delta}"
printf 'linked text+data delta: %+d B\n' "${load_delta}"
printf 'ELF bss delta: %+d B\n' "${bss_delta}"
printf 'fixed DRAM delta: %+d B\n' "${fixed_delta}"
printf 'projector object text/data/bss: %d / %d / %d B\n' \
  "${object_text}" "${object_data}" "${object_bss}"
printf 'projectChordQualityPitchSet Xtensa stack: %s B\n' "${stack_bytes}"
printf 'P1_RESOURCE profile=%s linked_load_delta=%+d fixed_dram_delta=%+d bss_delta=%+d object_text=%d object_data=%d object_bss=%d stack=%s\n' \
  "${PROFILE}" "${load_delta}" "${fixed_delta}" "${bss_delta}" \
  "${object_text}" "${object_data}" "${object_bss}" "${stack_bytes}"
