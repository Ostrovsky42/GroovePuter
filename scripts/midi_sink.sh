#!/usr/bin/env bash
set -euo pipefail

# Keeps the Cardputer USB-MIDI IN endpoint drained on the host side.
#
# Without a process holding the ALSA port open, Linux never reads the device's
# bulk IN endpoint. The TinyUSB TX FIFO (16 event packets) then fills, every
# write fails, and the firmware parks playback in "USB WAIT - NO RECEIVER".
# That is the single most common cause of "playback stopped" during debugging:
# the serial monitor occupies CDC, but nothing consumes MIDI.
#
# Usage:
#   ./scripts/midi_sink.sh              # auto-detect port, print every message
#   ./scripts/midi_sink.sh -q           # drain silently (counts messages)
#   ./scripts/midi_sink.sh --list       # show all ALSA sequencer ports
#   ./scripts/midi_sink.sh 24:0         # pin an explicit client:port
#   ./scripts/midi_sink.sh --bridge     # route Cardputer -> SEQTRAK through this PC
#   ./scripts/midi_sink.sh --bridge=MyDevice
#   MIDI_PORT_PATTERN=MyName ./scripts/midi_sink.sh
#
# --bridge is the only way to test the SEQTRAK path: both the Cardputer and the
# SEQTRAK are USB devices, so they cannot see each other directly. The PC is the
# host for both and forwards one to the other with aconnect, which also drains
# the Cardputer endpoint.

QUIET=0
EXPLICIT_PORT=""
BRIDGE=0
BRIDGE_PATTERN="SEQTRAK"

for arg in "$@"; do
  case "${arg}" in
    -q|--quiet) QUIET=1 ;;
    --bridge) BRIDGE=1 ;;
    --bridge=*) BRIDGE=1; BRIDGE_PATTERN="${arg#--bridge=}" ;;
    --list)
      aseqdump -l
      exit 0
      ;;
    -h|--help)
      sed -n '3,20p' "$0"
      exit 0
      ;;
    *[0-9]:[0-9]*) EXPLICIT_PORT="${arg}" ;;
    *) echo "Unknown argument: ${arg}" >&2; exit 2 ;;
  esac
done

if ! command -v aseqdump >/dev/null 2>&1; then
  echo "aseqdump not found. Install alsa-utils." >&2
  exit 127
fi

find_port() {
  local pattern="$1"
  aseqdump -l | awk -v pat="${pattern}" '
    tolower($0) ~ tolower(pat) && $1 ~ /^[0-9]+:[0-9]+$/ { print $1; exit }'
}

PORT="${EXPLICIT_PORT}"
if [[ -z "${PORT}" ]]; then
  for candidate in "${MIDI_PORT_PATTERN:-}" GroovePuter MiniAcid Cardputer M5Stack ESP32; do
    [[ -z "${candidate}" ]] && continue
    PORT="$(find_port "${candidate}" || true)"
    if [[ -n "${PORT}" ]]; then
      echo "Found MIDI port ${PORT} matching '${candidate}'"
      break
    fi
  done
fi

if [[ -z "${PORT}" ]]; then
  echo "No Cardputer MIDI port found. Available ports:" >&2
  aseqdump -l >&2
  echo >&2
  echo "Plug the Cardputer into this machine's USB, or pass the port explicitly:" >&2
  echo "  ./scripts/midi_sink.sh 24:0" >&2
  exit 1
fi

if [[ "${BRIDGE}" -eq 1 ]]; then
  if ! command -v aconnect >/dev/null 2>&1; then
    echo "aconnect not found. Install alsa-utils." >&2
    exit 127
  fi
  DEST="$(find_port "${BRIDGE_PATTERN}" || true)"
  if [[ -z "${DEST}" ]]; then
    echo "No destination port matching '${BRIDGE_PATTERN}'. Available ports:" >&2
    aseqdump -l >&2
    echo >&2
    echo "Connect the SEQTRAK to this PC over USB, or pass a pattern:" >&2
    echo "  ./scripts/midi_sink.sh --bridge=MyDevice" >&2
    exit 1
  fi
  if [[ "${DEST%%:*}" == "${PORT%%:*}" ]]; then
    echo "Source and destination resolved to the same client (${PORT})." >&2
    echo "Pass an explicit destination pattern with --bridge=..." >&2
    exit 1
  fi

  aconnect "${PORT}" "${DEST}"
  # Leaving a subscription behind would keep forwarding after this script exits.
  trap 'aconnect -d "${PORT}" "${DEST}" >/dev/null 2>&1 || true' EXIT INT TERM
  echo "Bridging ${PORT} -> ${DEST} (${BRIDGE_PATTERN}). Ctrl-C to disconnect."
fi

echo "Draining ${PORT} — keep this running while testing playback. Ctrl-C to stop."

if [[ "${QUIET}" -eq 1 ]]; then
  # Still a real reader: ALSA only drains the endpoint while the port is open.
  aseqdump -p "${PORT}" | awk '
    NR % 100 == 0 { printf("received %d messages\n", NR); fflush() }'
else
  # Deliberately not exec: the bridge teardown trap must still run on Ctrl-C.
  aseqdump -p "${PORT}"
fi
