#!/usr/bin/env bash
set -euo pipefail

DURATION="${1:-60}"
PORT="${2:-${PORT:-/dev/ttyACM0}}"
LOG_DIR="${LOG_DIR:-logs}"
RETRY_DELAY_SECONDS="${RETRY_DELAY_SECONDS:-0.25}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${DURATION}" != "0" && "${DURATION}" != "60" && "${DURATION}" != "180" ]]; then
  echo "Usage: $0 [0|60|180] [/dev/ttyACM0]" >&2
  echo "  0 monitors until Ctrl-C; 60 and 180 create bounded windows." >&2
  exit 2
fi

if ! python3 -c 'import serial' >/dev/null 2>&1; then
  echo "PySerial is required: python3 -m pip install pyserial" >&2
  exit 127
fi

mkdir -p "${LOG_DIR}"
STAMP="$(date +%Y%m%d-%H%M%S)"
WINDOW_LABEL="${DURATION}s"
if [[ "${DURATION}" == "0" ]]; then WINDOW_LABEL="continuous"; fi
LOG_FILE="${LOG_DIR}/serial-${WINDOW_LABEL}-${STAMP}.log"

if [[ "${DURATION}" == "0" ]]; then
  echo "Monitoring ${PORT} until Ctrl-C"
else
  echo "Monitoring ${PORT} for ${DURATION} seconds"
fi
echo "Log: ${LOG_FILE}"
echo "Press Ctrl-C to stop early."
echo "DTR=on, RTS=off; USB reconnects will be retried without auto-reset."
: > "${LOG_FILE}"

python3 "${SCRIPT_DIR}/serial_monitor.py" \
  --port "${PORT}" \
  --baud 115200 \
  --duration "${DURATION}" \
  --retry-delay "${RETRY_DELAY_SECONDS}" \
  --log "${LOG_FILE}"

echo "Monitoring finished. Saved: ${LOG_FILE}"
