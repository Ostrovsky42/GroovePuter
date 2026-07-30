#!/usr/bin/env bash
set -euo pipefail

ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
M5STACK_INDEX_URL="${M5STACK_INDEX_URL:-https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json}"
M5STACK_CORE_VERSION="${M5STACK_CORE_VERSION:-3.2.2}"
M5CARDPUTER_VERSION="${M5CARDPUTER_VERSION:-1.1.0}"
M5UNIFIED_VERSION="${M5UNIFIED_VERSION:-0.2.8}"
M5GFX_VERSION="${M5GFX_VERSION:-0.2.10}"

if ! command -v "${ARDUINO_CLI}" >/dev/null 2>&1; then
  echo "arduino-cli was not found: ${ARDUINO_CLI}" >&2
  exit 127
fi

"${ARDUINO_CLI}" core update-index \
  --additional-urls "${M5STACK_INDEX_URL}"
"${ARDUINO_CLI}" core install "m5stack:esp32@${M5STACK_CORE_VERSION}" \
  --additional-urls "${M5STACK_INDEX_URL}" \
  --run-post-install

# Arduino CLI installs declared transitive dependencies by default. Explicit
# versions keep the Cardputer ADV hardware layer reproducible.
"${ARDUINO_CLI}" lib update-index
"${ARDUINO_CLI}" lib install \
  "M5Cardputer@${M5CARDPUTER_VERSION}" \
  "M5Unified@${M5UNIFIED_VERSION}" \
  "M5GFX@${M5GFX_VERSION}"

"${ARDUINO_CLI}" core list
"${ARDUINO_CLI}" lib list | grep -E '^(M5Cardputer|M5Unified|M5GFX)[[:space:]]'
