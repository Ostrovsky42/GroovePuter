#!/usr/bin/env python3
from pathlib import Path
import sys
import re

if len(sys.argv) != 2:
    raise SystemExit("usage: instrument_cardputer_memory_runtime.py <source-root>")

root = Path(sys.argv[1])
ino_path = root / "GroovePuter.ino"
smf_header_path = root / "src/platform/cardputer_smf_player.h"
registry_header_path = root / "src/platform/cardputer_smf_player_registry.h"
registry_cpp_path = root / "src/platform/cardputer_smf_player_registry.cpp"
transport_header_path = root / "src/platform/cardputer_usb_midi_transport.h"
transport_cpp_path = root / "src/platform/cardputer_usb_midi_transport.cpp"
phrase_page_path = root / "src/ui/pages/phrase_page.cpp"
pattern_edit_path = root / "src/ui/pages/pattern_edit_page.cpp"
generation_commit_path = root / "src/generation/migration/quantized_generation_commit_impl.h"
scene_storage_path = root / "scene_storage_cardputer.cpp"
pattern_paging_path = root / "src/audio/pattern_paging.cpp"


# Keep the same probes usable on both the accepted baseline and the staged
# load pipeline. Only adapt exact load-expression anchors; never change logic.
_scene_load_source = scene_storage_path.read_text(encoding="utf-8")
_scene_prepared_load = "const SceneSynthPreparation& prepare" in _scene_load_source
_scene_staged_load = "manager.loadSceneEvented(filtered, true," in _scene_load_source
_scene_selection_publication = "bool publishSelection" in _scene_load_source

def scene_load_anchor(text: str) -> str:
    if _scene_prepared_load:
        text = text.replace("SceneManager& manager)",
            "SceneManager& manager, const SceneSynthPreparation& prepare)")
    if _scene_selection_publication:
        text = text.replace(
            "SceneManager& manager, const SceneSynthPreparation& prepare)",
            "SceneManager& manager, const SceneSynthPreparation& prepare, bool publishSelection)")
    if _scene_staged_load:
        suffix = ", prepare" if _scene_prepared_load else ""
        selection = "finishSceneRead(filtered, publishSelection)" if _scene_selection_publication else "!filtered.failed()"
        backup_selection = "finishSceneRead(filteredBackup, publishSelection)" if _scene_selection_publication else "!filteredBackup.failed()"
        text = text.replace("manager.loadSceneEvented(filtered) && !filtered.failed()",
            "manager.loadSceneEvented(filtered, true, [&]() { return " + selection + "; }" + suffix + ")")
        text = re.sub(r"manager\.loadSceneEvented\(filteredBackup\)\s*&&\s*!filteredBackup\.failed\(\)",
            "manager.loadSceneEvented(filteredBackup, true, [&]() { return " + backup_selection + "; }" + suffix + ")", text)
    return text


def replace_once(path: Path, anchor: str, replacement: str, label: str) -> None:
    if path == scene_storage_path:
        anchor = scene_load_anchor(anchor)
        replacement = scene_load_anchor(replacement)
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

# PHRASE probe hooks are declared locally so this file needs no diagnostic
# header. Both edits apply to the temporary runtime tree only.
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

# The generation call is a single initialising statement with one exit, so the
# end hook needs no change to existing error handling.
replace_once(
    phrase_page_path,
    "      });\n\n  GP_PROBE(\"G\");\n",
    "      });\n"
    "  endPhraseMemoryProbe(static_cast<int>(result.status));\n"
    "\n  GP_PROBE(\"G\");\n",
    "PHRASE probe end hook",
)

# A single G press can report the same toast for a busy pending activation and
# for a rejected musical candidate. Keep the diagnostic confined to runtime
# copies so the next hardware reproduction exposes the actual boundary.
replace_once(
    pattern_edit_path,
    '    const char* label = "GEN FAILED";\n',
    '    if (result == QuantizedGenerationResult::Failed) {\n'
    '      const auto& genre = mini_acid_.sceneManager().currentScene().genre;\n'
    '      Serial.printf("[GEN-DIAG] voice=%d playing=%u result=%u status=%u "\n'
    '                    "genre=%u recipe=%u page=%d\\n",\n'
    '                    voice_index_, static_cast<unsigned>(mini_acid_.isPlaying()),\n'
    '                    static_cast<unsigned>(result),\n'
    '                    static_cast<unsigned>(\n'
    '                        GroovePuterRhythm::quantizedGenerationStatus()),\n'
    '                    static_cast<unsigned>(genre.generativeMode),\n'
    '                    static_cast<unsigned>(genre.recipe),\n'
    '                    mini_acid_.sceneManager().currentPageIndex());\n'
    '    }\n'
    '    const char* label = "GEN FAILED";\n',
    "Pattern G result diagnostic",
)
replace_once(
    generation_commit_path,
    '  const StrongRhythmMigrationResult migration = migrateStrongRhythmSynths(\n'
    '      requestedGenre,\n'
    '      context,\n'
    '      candidate.drums,\n'
    '      candidate.synth[0],\n'
    '      candidate.synth[1]);\n',
    '  const StrongRhythmMigrationResult migration = migrateStrongRhythmSynths(\n'
    '      requestedGenre,\n'
    '      context,\n'
    '      candidate.drums,\n'
    '      candidate.synth[0],\n'
    '      candidate.synth[1]);\n'
    '  if (migration.status != StrongRhythmMigrationStatus::Applied) {\n'
    '    Serial.printf("[GEN-DIAG] migration=%u route=%u address=%d "\n'
    '                  "feel=%u amount=%u root=%u scale=%u ordinal=%lu\\n",\n'
    '                  static_cast<unsigned>(migration.status),\n'
    '                  static_cast<unsigned>(migration.route),\n'
    '                  static_cast<int>(context.patternAddress),\n'
    '                  static_cast<unsigned>(context.feelProfile),\n'
    '                  static_cast<unsigned>(context.feelAmount),\n'
    '                  static_cast<unsigned>(context.rootPitchClass),\n'
    '                  static_cast<unsigned>(context.scaleTypeValue),\n'
    '                  static_cast<unsigned long>(generationAttemptOrdinal));\n'
    '  }\n',
    "Synth migration rejection diagnostic",
)

# E0 SD latency probes are runtime-build-only. They count SD.open() latency in
# scene and pattern storage, and direct File read/write latency at the shared
# serialization boundaries used by those paths.
replace_once(
    scene_storage_path,
    '#include "src/platform/cardputer_sd.h"\n',
    '#include "src/platform/cardputer_sd.h"\n'
    '#include "src/diag/cardputer_e0_sd_metrics.h"\n',
    "scene storage E0 metrics include",
)
replace_once(
    pattern_paging_path,
    "#include <SD.h>\n#else",
    '#include <SD.h>\n#include "../diag/cardputer_e0_sd_metrics.h"\n#else',
    "pattern paging E0 metrics include",
)
for path in (scene_storage_path, pattern_paging_path):
    source = path.read_text(encoding="utf-8")
    if "cardputerE0TimedSdOpen(" in source:
        raise SystemExit(
            f"memory baseline instrumentation: SD.open() already instrumented in {path}")
    instrumented = []
    open_count = 0
    for line in source.splitlines(keepends=True):
        offset = line.find("SD.open(")
        if offset < 0 or line.lstrip().startswith("//"):
            instrumented.append(line)
            continue
        if line.lstrip().startswith('LOG_DEBUG("[commitPageCandidate] SD.open('):
            instrumented.append(line)
            continue
        if '"' in line[:offset]:
            raise SystemExit(
                f"memory baseline instrumentation: review SD.open() in string at {path}: {line.strip()}")
        if line.count("SD.open(") != 1:
            raise SystemExit(
                f"memory baseline instrumentation: review SD.open() line in {path}: {line.strip()}")
        instrumented.append(line.replace("SD.open(", "cardputerE0TimedSdOpen(", 1))
        open_count += 1
    if open_count == 0:
        raise SystemExit(
            f"memory baseline instrumentation: expected SD.open() calls in {path}")
    path.write_text("".join(instrumented), encoding="utf-8")

replace_once(
    pattern_paging_path,
    "bool writeAll(File& file, const void* data, size_t length) {\n"
    "    return file.write(reinterpret_cast<const uint8_t*>(data), length) == length;\n"
    "}\n\n"
    "bool readAll(File& file, void* data, size_t length) {\n"
    "    return file.read(reinterpret_cast<uint8_t*>(data), length) == length;\n"
    "}",
    "bool writeAll(File& file, const void* data, size_t length) {\n"
    "    const uint32_t startedAt = micros();\n"
    "    const size_t written =\n"
    "        file.write(reinterpret_cast<const uint8_t*>(data), length);\n"
    "    cardputerE0RecordSdWrite(micros() - startedAt, written);\n"
    "    return written == length;\n"
    "}\n\n"
    "bool readAll(File& file, void* data, size_t length) {\n"
    "    const uint32_t startedAt = micros();\n"
    "    const size_t read = file.read(reinterpret_cast<uint8_t*>(data), length);\n"
    "    cardputerE0RecordSdRead(micros() - startedAt, read);\n"
    "    return read == length;\n"
    "}",
    "pattern page SD read/write latency hooks",
)

scene_save_anchor = (
    "  GroovePuterSampler::SamplerSceneWriteFilter<File> filtered(file);\n"
    "  const bool serialized = manager.writeSceneJson(filtered) && filtered.finish();\n"
    "  file.flush();\n"
    "  const size_t bytesWritten = file.size();"
)
scene_save_replacement = (
    "  GroovePuterSampler::SamplerSceneWriteFilter<File> filtered(file);\n"
    "  const uint32_t saveStartedAt = micros();\n"
    "  const bool serialized = manager.writeSceneJson(filtered) && filtered.finish();\n"
    "  file.flush();\n"
    "  const size_t bytesWritten = file.size();\n"
    "  cardputerE0RecordSceneSave(micros() - saveStartedAt, bytesWritten);"
)
scene_storage_text = scene_storage_path.read_text(encoding="utf-8")
scene_save_count = scene_storage_text.count(scene_save_anchor)
if scene_save_count != 2:
    raise SystemExit(
        "memory baseline instrumentation: expected two Scene save anchors "
        f"in {scene_storage_path}, found {scene_save_count}")
scene_storage_path.write_text(
    scene_storage_text.replace(scene_save_anchor, scene_save_replacement),
    encoding="utf-8")

replace_once(
    scene_storage_path,
    "bool SceneStorageCardputer::readSceneAuto(SceneManager& manager) {\n"
    "  if (!isInitialized_) return false;",
    "bool SceneStorageCardputer::readSceneAuto(SceneManager& manager) {\n"
    "  if (!isInitialized_) return false;\n"
    "  cardputerE0RecordSongLoaded(false);",
    "autosave load probe start",
)
replace_once(
    scene_storage_path,
    "    GroovePuterSampler::SamplerSceneReadFilter<File> filtered(autoFile);\n"
    "    const bool ok = manager.loadSceneEvented(filtered) && !filtered.failed();",
    "    GroovePuterSampler::SamplerSceneReadFilter<File> filtered(autoFile);\n"
    "    const uint32_t loadStartedAt = micros();\n"
    "    const bool ok = manager.loadSceneEvented(filtered) && !filtered.failed();\n"
    "    cardputerE0RecordSceneLoad(micros() - loadStartedAt, autoFile.size());",
    "autosave primary scene-load latency",
)
replace_once(
    scene_storage_path,
    "    autoFile.close();\n"
    "    if (ok) return true;",
    "    autoFile.close();\n"
    "    if (ok) {\n"
    "      cardputerE0RecordSongLoaded(true);\n"
    "      return true;\n"
    "    }",
    "autosave load probe success",
)
replace_once(
    scene_storage_path,
    "    autoBackupFile.close();\n"
    "    if (ok) return true;",
    "    autoBackupFile.close();\n"
    "    if (ok) {\n"
    "      cardputerE0RecordSongLoaded(true);\n"
    "      return true;\n"
    "    }",
    "autosave backup load probe success",
)
replace_once(
    scene_storage_path,
    "    GroovePuterSampler::SamplerSceneReadFilter<File> filteredBackup(\n"
    "        autoBackupFile);\n"
    "    const bool ok = manager.loadSceneEvented(filteredBackup) &&\n"
    "                    !filteredBackup.failed();",
    "    GroovePuterSampler::SamplerSceneReadFilter<File> filteredBackup(\n"
    "        autoBackupFile);\n"
    "    const uint32_t loadStartedAt = micros();\n"
    "    const bool ok = manager.loadSceneEvented(filteredBackup) &&\n"
    "                    !filteredBackup.failed();\n"
    "    cardputerE0RecordSceneLoad(micros() - loadStartedAt,\n"
    "                              autoBackupFile.size());",
    "autosave backup scene-load latency",
)

replace_once(
    scene_storage_path,
    "bool SceneStorageCardputer::readScene(SceneManager& manager) {\n"
    "  if (!isInitialized_) return false;",
    "bool SceneStorageCardputer::readScene(SceneManager& manager) {\n"
    "  if (!isInitialized_) return false;\n"
    "  cardputerE0RecordSongLoaded(false);",
    "song load probe start",
)
replace_once(
    scene_storage_path,
    "    const bool ok = manager.loadSceneEvented(filtered) && !filtered.failed();\n"
    "    file.close();\n"
    "    if (ok) return true;",
    "    const uint32_t loadStartedAt = micros();\n"
    "    const bool ok = manager.loadSceneEvented(filtered) && !filtered.failed();\n"
    "    cardputerE0RecordSceneLoad(micros() - loadStartedAt, file.size());\n"
    "    file.close();\n"
    "    cardputerE0RecordSongLoaded(ok);\n"
    "    if (ok) return true;",
    "song load probe success",
)
replace_once(
    scene_storage_path,
    "  const bool recovered = manager.loadSceneEvented(filteredBackup) &&\n"
    "                         !filteredBackup.failed();\n"
    "  backup.close();\n"
    "  return recovered;",
    "  const uint32_t loadStartedAt = micros();\n"
    "  const bool recovered = manager.loadSceneEvented(filteredBackup) &&\n"
    "                         !filteredBackup.failed();\n"
    "  cardputerE0RecordSceneLoad(micros() - loadStartedAt, backup.size());\n"
    "  backup.close();\n"
    "  cardputerE0RecordSongLoaded(recovered);\n"
    "  return recovered;",
    "song backup load probe and scene-load latency",
)

replace_once(
    ino_path,
    '#include "src/dsp/miniacid_engine.h"\n',
    '#include "src/dsp/miniacid_engine.h"\n#include "src/audio/pattern_paging.h"\n',
    "persistence diagnostic namespace accessor include",
)

text = ino_path.read_text(encoding="utf-8")

state_anchor = "static uint32_t g_peakUiDrawUs = 0;\n"
state_injection = r'''static uint32_t g_peakUiDrawUs = 0;

// Narrow forward declaration avoids importing the TinyUSB transport header into
// the main sketch translation unit and changing its include graph.
TaskHandle_t cardputerMidiDispatchTaskHandleForMemoryBaseline();
extern MiniAcid* volatile g_miniAcid;
extern MiniAcidDisplay* g_miniDisplay;

// Diagnostic-build-only memory watermarks. This code is injected into the
// temporary runtime source tree and is never part of the product image.
static bool g_memoryBaselineRuntimeStarted = false;
static uint32_t g_memoryBaselineMinFree8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineMinLargest8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineMinFreeInternal8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineMinLargestInternal8 = 0xFFFFFFFFu;
static uint32_t g_memoryBaselineLastSampleMs = 0;
static uint32_t g_memoryBaselineLastLogMs = 0;
static uint32_t g_e0SdOpenCount = 0;
static uint32_t g_e0SdOpenFailCount = 0;
static uint32_t g_e0SdOpenMaxUs = 0;
static uint32_t g_e0SdReadCount = 0;
static uint32_t g_e0SdReadMaxUs = 0;
static uint32_t g_e0SdReadBytes = 0;
static uint32_t g_e0SdWriteCount = 0;
static uint32_t g_e0SdWriteMaxUs = 0;
static uint32_t g_e0SdWriteBytes = 0;
static uint32_t g_e0SceneLoadCount = 0;
static uint32_t g_e0SceneLoadMaxUs = 0;
static uint32_t g_e0SceneLoadBytes = 0;
static uint32_t g_e0SceneSaveCount = 0;
static uint32_t g_e0SceneSaveMaxUs = 0;
static uint32_t g_e0SceneSaveBytes = 0;
static bool g_e0SongLoaded = false;

void cardputerE0RecordSdOpen(uint32_t elapsedUs, bool succeeded) {
  ++g_e0SdOpenCount;
  if (!succeeded) ++g_e0SdOpenFailCount;
  if (elapsedUs > g_e0SdOpenMaxUs) g_e0SdOpenMaxUs = elapsedUs;
}

void cardputerE0RecordSdRead(uint32_t elapsedUs, uint32_t bytes) {
  ++g_e0SdReadCount;
  g_e0SdReadBytes += bytes;
  if (elapsedUs > g_e0SdReadMaxUs) g_e0SdReadMaxUs = elapsedUs;
}

void cardputerE0RecordSdWrite(uint32_t elapsedUs, uint32_t bytes) {
  ++g_e0SdWriteCount;
  g_e0SdWriteBytes += bytes;
  if (elapsedUs > g_e0SdWriteMaxUs) g_e0SdWriteMaxUs = elapsedUs;
}

void cardputerE0RecordSceneLoad(uint32_t elapsedUs, uint32_t bytes) {
  ++g_e0SceneLoadCount;
  g_e0SceneLoadBytes += bytes;
  if (elapsedUs > g_e0SceneLoadMaxUs) g_e0SceneLoadMaxUs = elapsedUs;
}

void cardputerE0RecordSceneSave(uint32_t elapsedUs, uint32_t bytes) {
  ++g_e0SceneSaveCount;
  g_e0SceneSaveBytes += bytes;
  if (elapsedUs > g_e0SceneSaveMaxUs) g_e0SceneSaveMaxUs = elapsedUs;
}

void cardputerE0RecordSongLoaded(bool loaded) {
  g_e0SongLoaded = loaded;
}

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

  Serial.printf(
      "[E0-STATE] phase=%s ms=%u sdMounted=%u songLoaded=%u playing=%u "
      "uiPage=%d patternPage=%d sourceA=%s sourceB=%s\n",
      phase ? phase : "periodic",
      (unsigned)nowMs,
      (unsigned)(SD.cardType() != CARD_NONE),
      (unsigned)(g_e0SongLoaded ? 1 : 0),
      (unsigned)(g_miniAcid && g_miniAcid->isPlaying()),
      g_miniDisplay ? g_miniDisplay->currentPageIndex() : -1,
      g_miniAcid ? g_miniAcid->currentPageIndex() : -1,
      !g_miniAcid ? "NONE" :
          g_miniAcid->currentSequencedSource(0) ==
                  MiniAcid::SequencedSource::Phrase ? "MELODY" : "STEPS",
      !g_miniAcid ? "NONE" :
          g_miniAcid->currentSequencedSource(1) ==
                  MiniAcid::SequencedSource::Phrase ? "MELODY" : "STEPS");

  const auto* persistMelodyA = g_miniAcid ? g_miniAcid->readableWorkingMelody(0) : nullptr;
  const auto* persistMelodyB = g_miniAcid ? g_miniAcid->readableWorkingMelody(1) : nullptr;
  Serial.printf(
      "[PERSIST-STATE] project=%s barsA=%u eventsA=%u dirtyA=%u barsB=%u eventsB=%u dirtyB=%u\n",
      PatternPagingService::currentProjectName().c_str(),
      persistMelodyA ? static_cast<unsigned>(persistMelodyA->lengthTicks / PhraseRuntime::kTicksPerBar) : 0u,
      persistMelodyA ? static_cast<unsigned>(persistMelodyA->count) : 0u,
      static_cast<unsigned>(g_miniAcid && g_miniAcid->hasUnsavedWorkingMelody(0)),
      persistMelodyB ? static_cast<unsigned>(persistMelodyB->lengthTicks / PhraseRuntime::kTicksPerBar) : 0u,
      persistMelodyB ? static_cast<unsigned>(persistMelodyB->count) : 0u,
      static_cast<unsigned>(g_miniAcid && g_miniAcid->hasUnsavedWorkingMelody(1)));

  Serial.printf(
      "[E0-SD] phase=%s ms=%u openCount=%u openFail=%u openMaxUs=%u "
      "readCount=%u readMaxUs=%u readBytes=%u writeCount=%u "
      "writeMaxUs=%u writeBytes=%u sceneLoadCount=%u sceneLoadMaxUs=%u "
      "sceneLoadBytes=%u sceneSaveCount=%u sceneSaveMaxUs=%u "
      "sceneSaveBytes=%u longMelodyAllocs=NA(layer-absent)\n",
      phase ? phase : "periodic",
      (unsigned)nowMs,
      (unsigned)g_e0SdOpenCount,
      (unsigned)g_e0SdOpenFailCount,
      (unsigned)g_e0SdOpenMaxUs,
      (unsigned)g_e0SdReadCount,
      (unsigned)g_e0SdReadMaxUs,
      (unsigned)g_e0SdReadBytes,
      (unsigned)g_e0SdWriteCount,
      (unsigned)g_e0SdWriteMaxUs,
      (unsigned)g_e0SdWriteBytes,
      (unsigned)g_e0SceneLoadCount,
      (unsigned)g_e0SceneLoadMaxUs,
      (unsigned)g_e0SceneLoadBytes,
      (unsigned)g_e0SceneSaveCount,
      (unsigned)g_e0SceneSaveMaxUs,
      (unsigned)g_e0SceneSaveBytes);
}

// The first heap_caps_monitor_local_minimum_free_size_start() may allocate
// its own bookkeeping. Prewarm once here so that allocation lands before the
// runtime baseline and can never be charged to a PHRASE measurement window.
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

// GF2-M0R operation-scoped PHRASE probe.
//
// GeneratedPhraseSong::generate() runs synchronously on the loop task, so the
// 10 ms poller above cannot sample anything while it executes. These hooks
// bracket the operation instead. External linkage: phrase_page.cpp calls them
// from another translation unit.
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
  // Read the minimum while the local monitor is still active: after stop()
  // heap_caps_get_minimum_free_size() reverts to its since-startup meaning.
  const uint32_t localMinFreeInternal8 = heap_caps_get_minimum_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t postFreeInternal8 = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t postLargestInternal8 = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  // Captured before any Serial.printf so logging cannot move the mark.
  const uint32_t postLoopStackFreeBytes =
      (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
  (void)heap_caps_monitor_local_minimum_free_size_stop();

  // localMinFreeInternal8 is the PHRASE-scoped system minimum free
  // INTERNAL|8BIT heap. Other tasks run during the window, so it is an
  // operation-window safety figure, not bytes allocated by PHRASE.
  //
  // The largest-block values are plain before/after readings. ESP-IDF has no
  // operation-local monitor for the largest free block, so neither one is an
  // operation peak; fragmentation drift is read across repeated operations.
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

setup_anchor = '''  Serial.println("setup() complete");
  markBootStage(100, "setup-complete");'''
setup_injection = '''  Serial.println("setup() complete");
  startCardputerMemoryBaseline();
  markBootStage(100, "setup-complete");'''

loop_anchor = '''void loop() {
  // Diagnostic only; compiles to (void)0 unless GROOVEPUTER_MELODY_CENSUS is set.
  MELODY_CENSUS_TICK(g_miniAcid && g_miniAcid->isPlaying());
  M5Cardputer.update();'''
loop_injection = '''void loop() {
  // Diagnostic only; compiles to (void)0 unless GROOVEPUTER_MELODY_CENSUS is set.
  MELODY_CENSUS_TICK(g_miniAcid && g_miniAcid->isPlaying());
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
