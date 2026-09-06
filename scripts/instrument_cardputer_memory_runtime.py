#!/usr/bin/env python3
from pathlib import Path
import sys


def parse_args() -> tuple[str, Path]:
    args = sys.argv[1:]
    if len(args) == 1:
        return "legacy", Path(args[0])
    if len(args) == 3 and args[0] == "--mode" and args[1] in ("legacy", "r0"):
        return args[1], Path(args[2])
    raise SystemExit(
        "usage: instrument_cardputer_memory_runtime.py "
        "[--mode legacy|r0] <source-root>")


mode, root = parse_args()
ino_path = root / "GroovePuter.ino"
smf_header_path = root / "src/platform/cardputer_smf_player.h"
smf_cpp_path = root / "src/platform/cardputer_smf_player.cpp"
registry_header_path = root / "src/platform/cardputer_smf_player_registry.h"
registry_cpp_path = root / "src/platform/cardputer_smf_player_registry.cpp"
transport_header_path = root / "src/platform/cardputer_usb_midi_transport.h"
transport_cpp_path = root / "src/platform/cardputer_usb_midi_transport.cpp"
phrase_page_path = root / "src/ui/pages/phrase_page.cpp"


def replace_once(path: Path, anchor: str, replacement: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(anchor)
    if count != 1:
        raise SystemExit(
            f"memory baseline instrumentation: expected one {label} anchor "
            f"in {path}, found {count}")
    path.write_text(text.replace(anchor, replacement, 1), encoding="utf-8")


def replace_text_once(text: str, anchor: str, replacement: str, label: str) -> str:
    count = text.count(anchor)
    if count != 1:
        raise SystemExit(
            f"memory baseline instrumentation: expected one {label} anchor, "
            f"found {count}")
    return text.replace(anchor, replacement, 1)


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

# The historical runtime profile keeps the PHRASE operation probe exactly as it
# existed before MEMORY-R0. R0 deliberately does not patch this call path: its
# first hardware image observes product startup/idle behavior only.
if mode == "legacy":
    replace_once(
        phrase_page_path,
        '#include "src/state/scene_revision.h"\n',
        '#include "src/state/scene_revision.h"\n'
        "\n"
        "// GF2-M0R diagnostic hooks, defined in the instrumented sketch.\n"
        "void beginPhraseMemoryProbe();\n"
        "void endPhraseMemoryProbe(int resultStatus);\n",
        "PHRASE probe declarations",
    )

    replace_once(
        phrase_page_path,
        "  const GeneratedPhraseSong::Result result = GeneratedPhraseSong::generate(\n",
        "  beginPhraseMemoryProbe();\n"
        "  const GeneratedPhraseSong::Result result = GeneratedPhraseSong::generate(\n",
        "PHRASE probe begin hook",
    )

    replace_once(
        phrase_page_path,
        "      });\n\n  if (!result) {\n",
        "      });\n"
        "  endPhraseMemoryProbe(static_cast<int>(result.status));\n"
        "\n  if (!result) {\n",
        "PHRASE probe end hook",
    )

text = ino_path.read_text(encoding="utf-8")

state_anchor = "static uint32_t g_peakUiDrawUs = 0;\n"
state_injection = r'''static uint32_t g_peakUiDrawUs = 0;

// Narrow forward declaration avoids importing the TinyUSB transport header into
// the main sketch translation unit and changing its include graph.
TaskHandle_t cardputerMidiDispatchTaskHandleForMemoryBaseline();

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

// The first heap_caps_monitor_local_minimum_free_size_start() may allocate
// its own bookkeeping. Legacy PHRASE diagnostics prewarm it before use.
static void prewarmCardputerMemoryLocalMinimumMonitor() {
  (void)heap_caps_monitor_local_minimum_free_size_start();
  (void)heap_caps_monitor_local_minimum_free_size_stop();
}

static void startCardputerMemoryBaseline() {
  g_memoryBaselineRuntimeStarted = true;
  prewarmCardputerMemoryLocalMinimumMonitor();
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

// GF2-M0R operation-scoped PHRASE probe retained for legacy mode.
static uint32_t g_phraseProbeSeq = 0;
static uint32_t g_phraseProbePreFreeInternal8 = 0;
static uint32_t g_phraseProbePreLargestInternal8 = 0;
static uint32_t g_phraseProbePreLoopStackFreeBytes = 0;

void beginPhraseMemoryProbe() {
  g_phraseProbePreFreeInternal8 = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  g_phraseProbePreLargestInternal8 = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  g_phraseProbePreLoopStackFreeBytes =
      (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
  (void)heap_caps_monitor_local_minimum_free_size_start();
}

void endPhraseMemoryProbe(int resultStatus) {
  const uint32_t localMinFreeInternal8 = heap_caps_get_minimum_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t postFreeInternal8 = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t postLargestInternal8 = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t postLoopStackFreeBytes =
      (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
  (void)heap_caps_monitor_local_minimum_free_size_stop();

  Serial.printf(
      "[MEM-PHRASE] seq=%u result=%d preFreeInternal8=%u "
      "localMinFreeInternal8=%u postFreeInternal8=%u preLargestInternal8=%u "
      "postLargestInternal8=%u preLoopStackFreeBytes=%u "
      "postLoopStackFreeBytes=%u\n",
      (unsigned)(++g_phraseProbeSeq),
      resultStatus,
      (unsigned)g_phraseProbePreFreeInternal8,
      (unsigned)localMinFreeInternal8,
      (unsigned)postFreeInternal8,
      (unsigned)g_phraseProbePreLargestInternal8,
      (unsigned)postLargestInternal8,
      (unsigned)g_phraseProbePreLoopStackFreeBytes,
      (unsigned)postLoopStackFreeBytes);
}
'''

if mode == "r0":
    # The local-minimum monitor may allocate bookkeeping on first use. R0's
    # untouched-idle image must not introduce that allocation merely by starting
    # telemetry, so only legacy mode prewarms it.
    state_injection = state_injection.replace(
        "  prewarmCardputerMemoryLocalMinimumMonitor();\n", "", 1)

text = replace_text_once(text, state_anchor, state_injection, "state")

if mode == "legacy":
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

    text = replace_text_once(text, setup_anchor, setup_injection, "setup")
    text = replace_text_once(text, loop_anchor, loop_injection, "loop")

    # Historical P3 diagnostic scenario remains the default/legacy runtime
    # profile. MEMORY-R0 does not compile or execute this synthetic workload.
    keyscan_include_anchor = "static uint32_t g_peakUiDrawUs = 0;\n"
    keyscan_include_injection = """static uint32_t g_peakUiDrawUs = 0;

#define P3_KEY_SCAN_TRACE 1
#include "src/diag/p3_key_scan_trace.h"
"""

    keyscan_call_anchor = "  reconcilePerformanceKeys(currentKeysState);\n"
    keyscan_call_injection = """  reconcilePerformanceKeys(currentKeysState);
  P3KeyScanTrace::observe(currentKeysState);
"""

    p3_include_anchor = "static MiniAcid g_miniAcidInstance(kSampleRate, &g_sceneStorage);\n"
    p3_include_injection = """static MiniAcid g_miniAcidInstance(kSampleRate, &g_sceneStorage);

#define P3_DRAM_CHARACTERIZATION 1
#include "src/diag/p3_dram_characterization.h"
"""

    p3_setup_anchor = "  startCardputerMemoryBaseline();\n"
    p3_setup_injection = """  startCardputerMemoryBaseline();
  P3DramCharacterization::begin(g_miniAcidInstance);
"""

    p3_loop_anchor = "  pollCardputerMemoryBaseline();\n"
    p3_loop_injection = """  pollCardputerMemoryBaseline();
  if (const char* p3Phase = P3DramCharacterization::poll(g_miniAcidInstance)) {
    logCardputerMemoryBaseline(p3Phase);
  }
"""

    for anchor, replacement, label in (
        (p3_include_anchor, p3_include_injection, "p3-include"),
        (p3_setup_anchor, p3_setup_injection, "p3-setup"),
        (p3_loop_anchor, p3_loop_injection, "p3-loop"),
        (keyscan_include_anchor, keyscan_include_injection, "keyscan-include"),
        (keyscan_call_anchor, keyscan_call_injection, "keyscan-call"),
    ):
        text = replace_text_once(text, anchor, replacement, label)
else:
    # R0 SMF phase checkpoints split timing-vector residency from the task stack
    # without changing begin() order or ownership.
    replace_once(
        smf_cpp_path,
        '#include "src/platform/cardputer_usb_midi_service.h"\n',
        '#include "src/platform/cardputer_usb_midi_service.h"\n\n'
        'void logCardputerMemoryR0Checkpoint(const char* phase);\n',
        "R0 SMF checkpoint declaration",
    )
    replace_once(
        smf_cpp_path,
        "bool CardputerSmfPlayerService::begin() {\n"
        "    if (taskHandle_ != nullptr) return true;\n\n"
        "    const uint32_t freeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);",
        "bool CardputerSmfPlayerService::begin() {\n"
        "    if (taskHandle_ != nullptr) return true;\n\n"
        "    logCardputerMemoryR0Checkpoint(\"r0-smf-begin\");\n"
        "    const uint32_t freeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);",
        "R0 SMF begin",
    )
    replace_once(
        smf_cpp_path,
        "    if (!timing_.reserveForEvents(kMaxTimingEvents)) {",
        "    logCardputerMemoryR0Checkpoint(\"r0-after-smf-timing-document\");\n"
        "    if (!timing_.reserveForEvents(kMaxTimingEvents)) {",
        "R0 SMF timing document",
    )
    replace_once(
        smf_cpp_path,
        "        return false;\n"
        "    }\n\n"
        "    commandQueue_ = xQueueCreateStatic(\n",
        "        return false;\n"
        "    }\n"
        "    logCardputerMemoryR0Checkpoint(\"r0-after-smf-timing-map\");\n\n"
        "    commandQueue_ = xQueueCreateStatic(\n",
        "R0 SMF timing map",
    )
    replace_once(
        smf_cpp_path,
        "        return false;\n"
        "    }\n\n"
        "    const BaseType_t result = xTaskCreatePinnedToCore(\n",
        "        return false;\n"
        "    }\n"
        "    logCardputerMemoryR0Checkpoint(\"r0-after-smf-command-queue\");\n\n"
        "    const BaseType_t result = xTaskCreatePinnedToCore(\n",
        "R0 SMF command queue",
    )
    replace_once(
        smf_cpp_path,
        "    Serial.printf(\"[SMF-INIT] ready freeInt=%u largest=%u\\n\",",
        "    logCardputerMemoryR0Checkpoint(\"r0-after-smf-task\");\n"
        "    Serial.printf(\"[SMF-INIT] ready freeInt=%u largest=%u\\n\",",
        "R0 SMF task",
    )

    boot_helper_anchor = '''static void markBootStage(uint32_t stage, const char* msg = nullptr) {
  g_bootStage.record(stage);
  if (msg) {
    Serial.printf("[BOOT-STAGE] %u %s\\n", (unsigned)stage, msg);
  } else {
    Serial.printf("[BOOT-STAGE] %u\\n", (unsigned)stage);
  }
}
'''
    boot_helper_injection = boot_helper_anchor + '''
// R0 retained markers intentionally do not call markBootStage(): that helper
// writes serial text and would turn the loop into a tracer.
static void recordCardputerMemoryR0Stage(uint32_t stage) {
  g_bootStage.record(stage);
}

// External linkage lets the diagnostic-only SMF translation unit report its
// internal begin() boundaries through the same telemetry implementation.
void logCardputerMemoryR0Checkpoint(const char* phase) {
  logCardputerMemoryBaseline(phase);
}
'''
    text = replace_text_once(
        text, boot_helper_anchor, boot_helper_injection, "R0 retained helper")

    r0_replacements = (
        (
            '''  logHeapCaps("before-audio-task");
  startAudioTask();
  logHeapCaps("after-audio-task");''',
            '''  logHeapCaps("before-audio-task");
  logCardputerMemoryBaseline("r0-before-audio-task");
  startAudioTask();
  logCardputerMemoryBaseline("r0-after-audio-task");
  logHeapCaps("after-audio-task");''',
            "R0 AudioTask phase",
        ),
        (
            '''  markBootStage(86, "before critical DSP buffers");
  g_miniAcidInstance.preallocateConstrainedDelayBuffers();
  markBootStage(87, "after critical DSP buffers");''',
            '''  logCardputerMemoryBaseline("r0-before-dsp-buffers");
  markBootStage(86, "before critical DSP buffers");
  g_miniAcidInstance.preallocateConstrainedDelayBuffers();
  markBootStage(87, "after critical DSP buffers");
  logCardputerMemoryBaseline("r0-after-dsp-buffers");''',
            "R0 DSP phase",
        ),
        (
            '''  markBootStage(82, "before early SD init");
  g_sceneStorage.initializeStorage();
  markBootStage(83, "after early SD init");''',
            '''  logCardputerMemoryBaseline("r0-before-sd");
  markBootStage(82, "before early SD init");
  g_sceneStorage.initializeStorage();
  markBootStage(83, "after early SD init");
  logCardputerMemoryBaseline("r0-after-sd");''',
            "R0 SD phase",
        ),
        (
            '''  screenLog("4c. SMF Runtime...");
  markBootStage(84, "before SMF runtime init");''',
            '''  screenLog("4c. SMF Runtime...");
  logCardputerMemoryBaseline("r0-before-smf");
  markBootStage(84, "before SMF runtime init");''',
            "R0 SMF before",
        ),
        (
            '''  markBootStage(85, "after SMF runtime init");

  // Global MIDI settings must be restored before the dispatcher starts.''',
            '''  markBootStage(85, "after SMF runtime init");
  logCardputerMemoryBaseline("r0-after-smf");

  // Global MIDI settings must be restored before the dispatcher starts.''',
            "R0 SMF after",
        ),
        (
            '''  screenLog("4d. USB MIDI Runtime...");
  markBootStage(52, "before USB MIDI sink");''',
            '''  screenLog("4d. USB MIDI Runtime...");
  logCardputerMemoryBaseline("r0-before-midi-dispatch");
  markBootStage(52, "before USB MIDI sink");''',
            "R0 MIDI before",
        ),
        (
            '''  }

  screenLog("5. Creating Encoder8");''',
            '''  }
  logCardputerMemoryBaseline("r0-after-midi-dispatch");

  screenLog("5. Creating Encoder8");''',
            "R0 MIDI after",
        ),
        (
            '''  markBootStage(50, "before MiniAcid::init");
  g_miniAcidInstance.init();''',
            '''  logCardputerMemoryBaseline("r0-before-miniacid");
  markBootStage(50, "before MiniAcid::init");
  g_miniAcidInstance.init();''',
            "R0 MiniAcid before",
        ),
        (
            '''  markBootStage(51, "after MiniAcid::init");

  // Scan samples from SD card''',
            '''  markBootStage(51, "after MiniAcid::init");
  logCardputerMemoryBaseline("r0-after-miniacid");

  // Scan samples from SD card''',
            "R0 MiniAcid after",
        ),
        (
            '''  markBootStage(60, "before sample scan");
  g_miniAcid->sampleIndex.scanDirectory("/sd/samples");''',
            '''  logCardputerMemoryBaseline("r0-before-samples");
  markBootStage(60, "before sample scan");
  g_miniAcid->sampleIndex.scanDirectory("/sd/samples");''',
            "R0 samples before",
        ),
        (
            '''  Serial.println("7a. UI Instance Created");
  markBootStage(70, "before MiniAcidDisplay alloc");''',
            '''  logCardputerMemoryBaseline("r0-after-samples");
  Serial.println("7a. UI Instance Created");
  logCardputerMemoryBaseline("r0-before-ui-root");
  markBootStage(70, "before MiniAcidDisplay alloc");''',
            "R0 samples/UI before",
        ),
        (
            '''  markBootStage(71, "after MiniAcidDisplay alloc");
  Serial.println("7b. UI setAudioGuard");''',
            '''  markBootStage(71, "after MiniAcidDisplay alloc");
  logCardputerMemoryBaseline("r0-after-ui-root");
  Serial.println("7b. UI setAudioGuard");''',
            "R0 UI after",
        ),
        (
            '''  drawUI();
  markBootStage(95, "after first drawUI");
  Serial.println("setup() complete");''',
            '''  drawUI();
  markBootStage(95, "after first drawUI");
  logCardputerMemoryBaseline("r0-after-first-draw");
  Serial.println("setup() complete");''',
            "R0 first draw",
        ),
        (
            '''  Serial.println("setup() complete");
  markBootStage(100, "setup-complete");''',
            '''  Serial.println("setup() complete");
  logCardputerMemoryBaseline("r0-setup-complete");
  startCardputerMemoryBaseline();
  markBootStage(100, "setup-complete");''',
            "R0 setup complete",
        ),
        (
            '''void loop() {
  M5Cardputer.update();
  LedManager::instance().update();''',
            '''void loop() {
  recordCardputerMemoryR0Stage(110);
  M5Cardputer.update();
  LedManager::instance().update();
  recordCardputerMemoryR0Stage(112);''',
            "R0 loop enter/hardware",
        ),
        (
            '''  }

  if (g_encoder8) g_encoder8->update();''',
            '''  }
  recordCardputerMemoryR0Stage(114);

  if (g_encoder8) g_encoder8->update();''',
            "R0 control sync",
        ),
        (
            '''  previousKeysState = currentKeysState;
  hasPreviousKeysState = true;

  static unsigned long lastUIUpdate = 0;''',
            '''  previousKeysState = currentKeysState;
  hasPreviousKeysState = true;
  recordCardputerMemoryR0Stage(116);

  static unsigned long lastUIUpdate = 0;
  recordCardputerMemoryR0Stage(118);''',
            "R0 input/UI begin",
        ),
        (
            '''  static unsigned long lastMemLog = 0;''',
            '''  recordCardputerMemoryR0Stage(120);
  recordCardputerMemoryR0Stage(122);
  static unsigned long lastMemLog = 0;''',
            "R0 UI/service boundary",
        ),
        (
            '''  }

  delay(5);
}''',
            '''  }
  pollCardputerMemoryBaseline();
  recordCardputerMemoryR0Stage(124);
  recordCardputerMemoryR0Stage(126);

  delay(5);
}''',
            "R0 service/loop exit",
        ),
    )

    for anchor, replacement, label in r0_replacements:
        text = replace_text_once(text, anchor, replacement, label)

ino_path.write_text(text, encoding="utf-8")
