#!/usr/bin/env bash
set -euo pipefail

PROFILE="${1:-normal}"
case "${PROFILE}" in
  normal|midi-only) ;;
  *) echo "usage: measure_p2_chord_rhythm_resources.sh <normal|midi-only>" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE_BUILD="${ROOT}/build/p2-chord-rhythm-${PROFILE}-default"
PROBE_BUILD="${ROOT}/build/p2-chord-rhythm-${PROFILE}-retained"
LOOP_STACK="${ARDUINO_LOOP_STACK_SIZE:-12288}"
rm -rf "${BASE_BUILD}" "${PROBE_BUILD}"
COMMON_CPP_FLAGS="-DARDUINO_LOOP_STACK_SIZE=${LOOP_STACK} -fstack-usage"

BUILD_PATH="${BASE_BUILD}" bash "${ROOT}/scripts/build_cardputer_memory_baseline.sh" \
  "${PROFILE}" product --warnings all \
  --build-property "compiler.cpp.extra_flags=${COMMON_CPP_FLAGS}"
BUILD_PATH="${PROBE_BUILD}" bash "${ROOT}/scripts/build_cardputer_memory_baseline.sh" \
  "${PROFILE}" product --warnings all \
  --build-property "compiler.cpp.extra_flags=${COMMON_CPP_FLAGS}" \
  --build-property "compiler.c.elf.extra_flags=-ugrooveputerP2ChordRhythmTimelineProbe"

discover_tool() {
  local name="$1"
  local root="${ARDUINO_PACKAGES_ROOT:-${HOME}/.arduino15/packages}"
  local found
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
test -f "${base_elf}" && test -f "${probe_elf}"

base_nm="${BASE_BUILD}/p2.nm"
probe_nm="${PROBE_BUILD}/p2.nm"
"${NM_TOOL}" -C "${base_elf}" > "${base_nm}"
"${NM_TOOL}" -C "${probe_elf}" > "${probe_nm}"
if grep -qE 'grooveputerP2ChordRhythmTimelineProbe|realizeChordRhythmTimeline' "${base_nm}"; then
  echo "P2 timeline leaked into default product link" >&2
  exit 1
fi
grep -q 'grooveputerP2ChordRhythmTimelineProbe' "${probe_nm}"
grep -q 'realizeChordRhythmTimeline' "${probe_nm}"

read_size() { "${SIZE_TOOL}" "$1" | awk 'NR == 2 {print $1, $2, $3}'; }
read_fixed_dram() {
  "${SIZE_TOOL}" -A "$1" | awk '
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

mapfile -t objects < <(find "${PROBE_BUILD}/.arduino-build" -type f -name '*chord_rhythm_timeline.cpp.o' -print | sort)
(( ${#objects[@]} == 1 )) || { echo "Expected one P2 object, found ${#objects[@]}" >&2; exit 2; }
read -r object_text object_data object_bss < <(read_size "${objects[0]}")

mapfile -t stack_files < <(find "${PROBE_BUILD}/.arduino-build" -type f -name '*chord_rhythm_timeline*.su' -print | sort)
(( ${#stack_files[@]} > 0 )) || { echo "No P2 Xtensa stack file" >&2; exit 2; }
stack_line=""
for file in "${stack_files[@]}"; do
  stack_line="$(grep -m1 'realizeChordRhythmTimeline' "${file}" || true)"
  [[ -n "${stack_line}" ]] && break
done
[[ -n "${stack_line}" ]]
stack_bytes="$(awk -F '\t' '{print $2}' <<<"${stack_line}")"

case "${PROFILE}" in
  normal)
    expected_load=384
    expected_text=384
    expected_data=0
    ;;
  midi-only)
    expected_load=368
    expected_text=368
    expected_data=0
    ;;
esac
expected_fixed=0
expected_bss=0
expected_object_text=419
expected_object_data=0
expected_object_bss=0
expected_stack=64

(( load_delta == expected_load ))
(( text_delta == expected_text ))
(( data_delta == expected_data ))
(( fixed_delta == expected_fixed ))
(( bss_delta == expected_bss ))
(( object_text == expected_object_text ))
(( object_data == expected_object_data ))
(( object_bss == expected_object_bss ))
(( stack_bytes == expected_stack ))

printf 'default link retains P2 timeline: NO\n'
printf 'forced probe link retains P2 timeline: YES\n'
printf 'P2_RESOURCE profile=%s linked_load_delta=%+d text_delta=%+d data_delta=%+d fixed_dram_delta=%+d bss_delta=%+d object_text=%d object_data=%d object_bss=%d stack=%s\n' \
  "${PROFILE}" "${load_delta}" "${text_delta}" "${data_delta}" "${fixed_delta}" "${bss_delta}" \
  "${object_text}" "${object_data}" "${object_bss}" "${stack_bytes}"
