#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: instrument_cardputer_memory_runtime.py <source-root>")

root = Path(sys.argv[1])
ino_path = root / "GroovePuter.ino"
smf_header_path = root / "src/platform/cardputer_smf_player.h"
registry_header_path = root / "src/platform/cardputer_smf_player_registry.h"
registry_cpp_path = root / "src/platform/cardputer_smf_player_registry.cpp"
transport_header_path = root / "src/platform/cardputer_usb_midi_transport.h"
transport_cpp_path = root / "src/platform/cardputer_usb_midi_transport.cpp"


def replace_once(path: Path, anchor: str, replacement: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(anchor)
    if count != 1:
        raise SystemExit(
            f"memory baseline instrumentation: expected one {label} anchor "
            f"in {path}, found {count}")
    path.write_text(text.replace(anchor, replacement, 1), encoding="utf-8")


# Runtime-only direct accessors avoid optional task-name lookup. None of these
# edits touch the product checkout or product ELF.
replace_once(
    smf_header_path,
    "    bool begin();\n    ScheduledSmfMidiEventQueue& eventQueue() { return eventQueue_; }",
    "    bool begin();\n"
    "    ScheduledSmfMidiEventQueue& eventQueue() { return eventQueue_; }\n"
    "    TaskHandle_t memoryBaselineTaskHandle() const { return taskHandle_; }",
    "SMF task accessor",
)

replace_once(
    registry_header_path,
    "#pragma once\n",
    "#pragma once\n\n"
    "#include <freertos/FreeRTOS.h>\n"
    "#include <freertos/task.h>\n",
    "SMF registry FreeRTOS includes",
)
replace_once(
    registry_header_path,
    "bool beginCardputerSmfPlayerService();",
    "bool beginCardputerSmfPlayerService();\n"
    "TaskHandle_t cardputerSmfPlayerTaskHandleForMemoryBaseline();",
    "SMF registry accessor declaration",
)
replace_once(
    registry_cpp_path,
    "    bool begin() {\n        return ensureStarted();\n    }",
    "    bool begin() {\n"
    "        return ensureStarted();\n"
    "    }\n\n"
    "    TaskHandle_t taskHandleForMemoryBaseline() const {\n"
    "        return player_.memoryBaselineTaskHandle();\n"
    "    }",
    "lazy SMF accessor",
)
replace_once(
    registry_cpp_path,
    "bool beginCardputerSmfPlayerService() {\n    return g_smfPlayer.begin();\n}\n",
    "bool beginCardputerSmfPlayerService() {\n"
    "    return g_smfPlayer.begin();\n"
    "}\n\n"
    "TaskHandle_t cardputerSmfPlayerTaskHandleForMemoryBaseline() {\n"
    "    return g_smfPlayer.taskHandleForMemoryBaseline();\n"
    "}\n",
    "SMF registry accessor definition",
)

replace_once(
    transport_header_path,
    "#include <cstdint>\n",
    "#include <cstdint>\n"
    "#include <freertos/FreeRTOS.h>\n"
    "#include <freertos/task.h>\n",
    "dispatcher FreeRTOS includes",
)
transport_header_text = transport_header_path.read_text(encoding="utf-8")
transport_header_path.write_text(
    transport_header_text
    + "\nTaskHandle_t cardputerMidiDispatchTaskHandleForMemoryBaseline();\n",
    encoding="utf-8",
)
transport_cpp_text = transport_cpp_path.read_text(encoding="utf-8")
transport_cpp_path.write_text(
    transport_cpp_text
    + "\nTaskHandle_t cardputerMidiDispatchTaskHandleForMemoryBaseline() {\n"
      "    return g_dispatchTaskHandle;\n"
      "}\n",
    encoding="utf-8",
)

text = ino_path.read_text(encoding="utf-8")
include_anchor = '#include "src/platform/cardputer_smf_player_registry.h"\n'
include_replacement = (
    include_anchor
    + '#include "src/platform/cardputer_usb_midi_transport.h"\n'
)
if text.count(include_anchor) != 1:
    raise SystemExit("memory baseline instrumentation: SMF registry include anchor mismatch")
text = text.replace(include_anchor, include_replacement, 1)

state_anchor = "static uint32_t g_peakUiDrawUs = 0;\n"
state_injection = r'''static uint32_t g_peakUiDrawUs = 0;

// Diagnostic-build-only memory watermarks. This code is injected into the
// temporary runtime source tree and is never part of the product image.
static bool g_memoryBaselineRuntimeStarted = false;
static uint32_t g_memoryBaselineMinFree8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineMinLargest8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineMinFreeInternal8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineMinLargestInternal8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineLastSampleMs = 0;
static uint32_t g_memoryBaselineLastLogMs = 0;

static void sampleCardputerMemoryBaseline() {
  const uint32_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const uint32_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const uint32_t freeInternal8 = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largestInternal8 = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!g_memoryBaselineRuntimeStarted) return;

  if (free8 < g_memoryBaselineMinFree8) g_memoryBaselineMinFree8 = free8;
  if (largest8 < g_memoryBaselineMinLargest8) {
    g_memoryBaselineMinLargest8 = largest8;
  }
  if (freeInternal8 < g_memoryBaselineMinFreeInternal8) {
    g_memoryBaselineMinFreeInternal8 = freeInternal8;
  }
  if (largestInternal8 < g_memoryBaselineMinLargestInternal8) {
    g_memoryBaselineMinLargestInternal8 = largestInternal8;
  }
}

static void logCardputerMemoryBaseline(const char* phase) {
  sampleCardputerMemoryBaseline();
  const uint32_t nowMs = millis();
  const uint32_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const uint32_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const uint32_t freeInternal8 = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largestInternal8 = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t minFree8Boot = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  const uint32_t minFreeInternal8Boot = heap_caps_get_minimum_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  const TaskHandle_t smfTask =
      cardputerSmfPlayerTaskHandleForMemoryBaseline();
  const TaskHandle_t dispatchTask =
      cardputerMidiDispatchTaskHandleForMemoryBaseline();
  const UBaseType_t loopStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr);
  const UBaseType_t audioStackFreeBytes = g_audioTaskHandle
      ? uxTaskGetStackHighWaterMark(g_audioTaskHandle)
      : 0;
  const UBaseType_t smfStackFreeBytes = smfTask
      ? uxTaskGetStackHighWaterMark(smfTask)
      : 0;
  const UBaseType_t dispatchStackFreeBytes = dispatchTask
      ? uxTaskGetStackHighWaterMark(dispatchTask)
      : 0;
  const bool integrity = heap_caps_check_integrity_all(false);

  Serial.printf(
      "[MEM-BASE] phase=%s ms=%u free8=%u minFree8Boot=%u "
      "minFree8RuntimeSample=%u largest8=%u minLargest8RuntimeSample=%u "
      "freeInternal8=%u minFreeInternal8Boot=%u "
      "minFreeInternal8RuntimeSample=%u largestInternal8=%u "
      "minLargestInternal8RuntimeSample=%u integrity=%u\n",
      phase ? phase : "periodic",
      (unsigned)nowMs,
      (unsigned)free8,
      (unsigned)minFree8Boot,
      (unsigned)(g_memoryBaselineMinFree8 == 0xFFFFFFFFu
          ? free8 : g_memoryBaselineMinFree8),
      (unsigned)largest8,
      (unsigned)(g_memoryBaselineMinLargest8 == 0xFFFFFFFFu
          ? largest8 : g_memoryBaselineMinLargest8),
      (unsigned)freeInternal8,
      (unsigned)minFreeInternal8Boot,
      (unsigned)(g_memoryBaselineMinFreeInternal8 == 0xFFFFFFFFu
          ? freeInternal8 : g_memoryBaselineMinFreeInternal8),
      (unsigned)largestInternal8,
      (unsigned)(g_memoryBaselineMinLargestInternal8 == 0xFFFFFFFFu
          ? largestInternal8 : g_memoryBaselineMinLargestInternal8),
      (unsigned)(integrity ? 1 : 0));

  Serial.printf(
      "[MEM-STACK] phase=%s ms=%u loopStackFreeBytes=%u "
      "audioStackFreeBytes=%u smfStackFreeBytes=%u "
      "dispatchStackFreeBytes=%u smfTaskPresent=%u dispatchTaskPresent=%u\n",
      phase ? phase : "periodic",
      (unsigned)nowMs,
      (unsigned)loopStackFreeBytes,
      (unsigned)audioStackFreeBytes,
      (unsigned)smfStackFreeBytes,
      (unsigned)dispatchStackFreeBytes,
      (unsigned)(smfTask ? 1 : 0),
      (unsigned)(dispatchTask ? 1 : 0));
}

static void startCardputerMemoryBaseline() {
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

ino_path.write_text(text, encoding="utf-8")
