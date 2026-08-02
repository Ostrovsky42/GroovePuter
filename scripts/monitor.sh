#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DURATION="${1:-0}"
PORT="${2:-${PORT:-/dev/ttyACM0}}"

# No duration preserves the old monitor-until-Ctrl-C behavior. Passing 60 or
# 180 uses the same reconnect-safe implementation with a bounded log window.
exec "${SCRIPT_DIR}/monitor_window.sh" "${DURATION}" "${PORT}"
