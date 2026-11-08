#!/usr/bin/env bash
# MIDI Clock bench for GroovePuter on a Linux host (Cardputer over USB, role
# COMPUTER). See README.md.
#
#   bench.sh follow BPM [SECONDS] [PREROLL]   laptop is the master; TEMPO CLOCK = MIDI IN
#   bench.sh master [SECONDS]                 GroovePuter is the master; CLOCK = INTERNAL
set -euo pipefail

case "${1:-}" in
  follow|master) ;;
  *) sed -n '2,6p' "$0" >&2; exit 2 ;;
esac

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/../.." && pwd)"
OUT="${MIDI_BENCH_OUT:-${ROOT}/build/midi_bench}"
mkdir -p "${OUT}"

cc -O2 -Wall -Wno-unused-result -Wno-misleading-indentation \
  -o "${OUT}/clockmaster" "${HERE}/clockmaster.c"
cc -O2 -Wall -Wno-unused-result -o "${OUT}/listen" "${HERE}/listen.c"

port="${MIDI_BENCH_PORT:-}"
if [[ -z "${port}" ]]; then
  hw="$(amidi -l | awk '/Cardputer/ {print $2; exit}')"
  if [[ -z "${hw}" ]]; then
    echo "No Cardputer MIDI port. Replug USB (role COMPUTER) and retry." >&2
    exit 1
  fi
  card="${hw#hw:}"; card="${card%%,*}"
  dev="${hw#hw:*,}"; dev="${dev%%,*}"
  port="/dev/snd/midiC${card}D${dev}"
fi

stamp="$(date +%Y%m%d-%H%M%S)"
case "${1:-}" in
  follow)
    bpm="${2:?BPM}"; secs="${3:-60}"; preroll="${4:-3}"
    log="${OUT}/follow-${bpm}-${stamp}.tsv"
    # Drain what queued while nobody read the port.
    "${OUT}/listen" "${port}" 1 > /dev/null
    "${OUT}/clockmaster" "${port}" "${bpm}" "${secs}" "${preroll}" > "${log}"
    if grep -q $'^RX\t.*\tRT F8' "${log}"; then
      echo "GroovePuter sent its own clock: TEMPO CLOCK is INTERNAL, not MIDI IN." >&2
    fi
    python3 -I "${HERE}/follow_offsets.py" "${log}" "${bpm}"
    python3 -I "${HERE}/bar_offsets.py" "${log}" "${bpm}"
    ;;
  master)
    secs="${2:-40}"
    log="${OUT}/master-${stamp}.tsv"
    echo "Listening ${secs} s on ${port}: press Stop, then Play on GroovePuter now."
    "${OUT}/listen" "${port}" "${secs}" > "${log}"
    python3 -I "${HERE}/master_offsets.py" "${log}"
    ;;
  *)
    sed -n '2,6p' "$0" >&2
    exit 2
    ;;
esac
echo "log: ${log}"
