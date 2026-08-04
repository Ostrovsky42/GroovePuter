#!/usr/bin/env bash
set -euo pipefail

PROFILE="${1:-normal}"
case "${PROFILE}" in
  normal|midi-only) shift || true ;;
  *)
    echo "usage: build_cardputer_memory_baseline.sh [normal|midi-only] [arduino-cli compile args...]" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SOURCE_COMMIT="$(git -C "${PROJECT_ROOT}" rev-parse HEAD 2>/dev/null || printf 'unknown')"
TEMP_ROOT="$(mktemp -d /tmp/grooveputer-memory-baseline.XXXXXX)"
SOURCE_ROOT="${TEMP_ROOT}/GroovePuter"

cleanup() {
  rm -rf "${TEMP_ROOT}"
}
trap cleanup EXIT

mkdir -p "${SOURCE_ROOT}"
rsync -a --delete \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'platform_sdl/build' \
  "${PROJECT_ROOT}/" "${SOURCE_ROOT}/"

# Instrument only the temporary build copy. The checkout and product firmware
# source remain untouched.
python3 - "${SOURCE_ROOT}/GroovePuter.ino" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

state_anchor = "static uint32_t g_peakUiDrawUs = 0;\n"
state_injection = r'''static uint32_t g_peakUiDrawUs = 0;

// Diagnostic-build-only memory watermarks. This code is injected into the
// temporary source tree by build_cardputer_memory_baseline.sh and is never part
// of the ordinary firmware build.
static bool g_memoryBaselineRuntimeStarted = false;
static uint32_t g_memoryBaselineStartBootFloor = 0;
static uint32_t g_memoryBaselineRuntimeMinFreeSampled = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineRuntimeMinLargestSampled = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineLastSampleMs = 0;
static uint32_t g_memoryBaselineLastLogMs = 0;

static void sampleCardputerMemoryBaseline() {
  const uint32_t freeInt = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largestInt = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (g_memoryBaselineRuntimeStarted) {
    if (freeInt < g_memoryBaselineRuntimeMinFreeSampled) {
      g_memoryBaselineRuntimeMinFreeSampled = freeInt;
    }
    if (largestInt < g_memoryBaselineRuntimeMinLargestSampled) {
      g_memoryBaselineRuntimeMinLargestSampled = largestInt;
    }
  }
}

static void logCardputerMemoryBaseline(const char* phase) {
  sampleCardputerMemoryBaseline();
  const uint32_t freeInt = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largestInt = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t bootFloor = heap_caps_get_minimum_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const UBaseType_t loopStackWords = uxTaskGetStackHighWaterMark(nullptr);
  const UBaseType_t audioStackWords = g_audioTaskHandle
      ? uxTaskGetStackHighWaterMark(g_audioTaskHandle)
      : 0;
  const bool integrity = heap_caps_check_integrity_all(false);
  Serial.printf(
      "[MEM-BASE] phase=%s ms=%u freeInt=%u minFreeBoot=%u "
      "startBootFloor=%u minFreeRuntimeSample=%u largest=%u "
      "minLargestRuntimeSample=%u loopStackWords=%u audioStackWords=%u "
      "integrity=%u\n",
      phase ? phase : "periodic",
      (unsigned)millis(),
      (unsigned)freeInt,
      (unsigned)bootFloor,
      (unsigned)g_memoryBaselineStartBootFloor,
      (unsigned)(g_memoryBaselineRuntimeMinFreeSampled == 0xFFFFFFFFu
          ? freeInt : g_memoryBaselineRuntimeMinFreeSampled),
      (unsigned)largestInt,
      (unsigned)(g_memoryBaselineRuntimeMinLargestSampled == 0xFFFFFFFFu
          ? largestInt : g_memoryBaselineRuntimeMinLargestSampled),
      (unsigned)loopStackWords,
      (unsigned)audioStackWords,
      (unsigned)(integrity ? 1 : 0));
}

static void startCardputerMemoryBaseline() {
  g_memoryBaselineStartBootFloor = heap_caps_get_minimum_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  g_memoryBaselineRuntimeStarted = true;
  sampleCardputerMemoryBaseline();
  logCardputerMemoryBaseline("runtime-start");
}

static void pollCardputerMemoryBaseline() {
  const uint32_t nowMs = millis();
  if (static_cast<int32_t>(nowMs - g_memoryBaselineLastSampleMs) >= 10) {
    g_memoryBaselineLastSampleMs = nowMs;
    sampleCardputerMemoryBaseline();
  }
  if (static_cast<int32_t>(nowMs - g_memoryBaselineLastLogMs) >= 1000) {
    g_memoryBaselineLastLogMs = nowMs;
    logCardputerMemoryBaseline("periodic");
  }
}
'''

setup_anchor = '''  Serial.println("setup() complete");
  markBootStage(100, "setup-complete");'''
setup_injection = '''  Serial.println("setup() complete");
  startCardputerMemoryBaseline();
  markBootStage(100, "setup-complete");'''

loop_anchor = '''void loop() {
  M5Cardputer.update();'''
loop_injection = '''void loop() {
  M5Cardputer.update();
  pollCardputerMemoryBaseline();'''

for anchor, replacement, label in (
    (state_anchor, state_injection, "state"),
    (setup_anchor, setup_injection, "setup"),
    (loop_anchor, loop_injection, "loop"),
):
    count = text.count(anchor)
    if count != 1:
        raise SystemExit(
            f"memory baseline instrumentation: expected one {label} anchor, found {count}")
    text = text.replace(anchor, replacement, 1)

path.write_text(text, encoding="utf-8")
PY

case "${PROFILE}" in
  normal)
    export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-memory-baseline}"
    ;;
  midi-only)
    export FQBN="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc"
    export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-memory-baseline-midi-only}"
    ;;
esac

printf 'Memory baseline profile: %s\n' "${PROFILE}"
printf 'Source commit: %s\n' "${SOURCE_COMMIT}"
printf 'FQBN: %s\n' "${FQBN}"
printf 'Temporary source: %s\n' "${SOURCE_ROOT}"
printf 'Build output: %s\n' "${BUILD_PATH}"

bash "${SOURCE_ROOT}/scripts/build.sh" "$@"

EXPECTED_ELF="${BUILD_PATH}/GroovePuter.ino.elf"
if [[ -f "${EXPECTED_ELF}" ]]; then
  ELF_PATH="${EXPECTED_ELF}"
else
  mapfile -t ELF_CANDIDATES < <(
    find "${BUILD_PATH}" -maxdepth 2 -type f -name '*.elf' -print | sort
  )
  if (( ${#ELF_CANDIDATES[@]} != 1 )); then
    printf 'Expected one ELF below %s, found %d\n' \
      "${BUILD_PATH}" "${#ELF_CANDIDATES[@]}" >&2
    printf '  %s\n' "${ELF_CANDIDATES[@]:-<none>}" >&2
    exit 2
  fi
  ELF_PATH="${ELF_CANDIDATES[0]}"
fi

ELF_SHA256="$(sha256sum "${ELF_PATH}" | awk '{print $1}')"
printf 'ELF path: %s\n' "${ELF_PATH}"
printf 'ELF sha256: %s\n' "${ELF_SHA256}"
bash "${SOURCE_ROOT}/scripts/report_cardputer_memory_baseline.sh" "${ELF_PATH}"

cat <<EOF

The report above is non-gating. The product gate is provisionally restored to
191488 bytes while PR #70 derives profile-specific policy from hardware data.

Flash normal diagnostic build:
  BUILD_PATH=${BUILD_PATH} bash scripts/upload.sh /dev/ttyACM0
EOF
