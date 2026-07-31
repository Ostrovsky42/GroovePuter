#!/usr/bin/env bash
set -euo pipefail

DURATION="${1:-60}"
PORT="${2:-${PORT:-/dev/ttyACM0}}"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
LOG_DIR="${LOG_DIR:-logs}"

if [[ "${DURATION}" != "60" && "${DURATION}" != "180" ]]; then
  echo "Usage: $0 [60|180] [/dev/ttyACM0]" >&2
  exit 2
fi

if ! command -v "${ARDUINO_CLI}" >/dev/null 2>&1; then
  echo "arduino-cli was not found: ${ARDUINO_CLI}" >&2
  echo "Set ARDUINO_CLI=/absolute/path/to/arduino-cli if needed." >&2
  exit 127
fi

mkdir -p "${LOG_DIR}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/serial-${DURATION}s-${STAMP}.log"

echo "Monitoring ${PORT} for ${DURATION} seconds"
echo "Log: ${LOG_FILE}"
echo "Press Ctrl-C to stop early."

set +e
timeout --foreground --signal=INT --kill-after=3s "${DURATION}s" \
  "${ARDUINO_CLI}" monitor \
    --port "${PORT}" \
    --config baudrate=115200 2>&1 | tee "${LOG_FILE}"
MONITOR_STATUS="${PIPESTATUS[0]}"
set -e

# timeout returns 124 when the requested observation window ends and 130 when
# the user stops the monitor with Ctrl-C; both are normal completion states.
if [[ "${MONITOR_STATUS}" != "0" && "${MONITOR_STATUS}" != "124" && "${MONITOR_STATUS}" != "130" ]]; then
  echo "Serial monitor failed with status ${MONITOR_STATUS}" >&2
  exit "${MONITOR_STATUS}"
fi

echo "Monitoring finished. Saved: ${LOG_FILE}"
