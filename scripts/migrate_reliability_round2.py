#!/usr/bin/env python3
"""Apply the second guarded GroovePuter reliability migration.

Every structural replacement is checked. A source drift aborts the job before
any commit is created.
"""

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 occurrence, found {count}")
    return text.replace(old, new, 1)


def replace_function(
    text: str, start_signature: str, next_signature: str, replacement: str
) -> str:
    start = text.index(start_signature)
    end = text.index(next_signature, start)
    return text[:start] + replacement.rstrip() + "\n\n" + text[end:]


def patch_pattern_paging() -> None:
    path = Path("src/audio/pattern_paging.cpp")
    text = path.read_text(encoding="utf-8")
    marker = "bool PatternPagingService::pageExists(int pageIndex) {"
    implementation = """void PatternPagingService::initializeEmptyPage(Scene& scene) {
    for (int bank = 0; bank < kBankCount; ++bank) {
        scene.synthABanks[bank] = Bank<SynthPattern>{};
        scene.synthBBanks[bank] = Bank<SynthPattern>{};
        scene.drumBanks[bank] = Bank<DrumPatternSet>{};
    }
}

""" + marker
    text = replace_once(text, marker, implementation, "empty page initializer")
    path.write_text(text, encoding="utf-8")


def patch_display_paging() -> None:
    path = Path("src/ui/miniacid_display.cpp")
    text = path.read_text(encoding="utf-8")
    start = text.index("void MiniAcidDisplay::handlePaging_() {")
    replacement = r'''void MiniAcidDisplay::handlePaging_() {
    if (!mini_acid_.isPageLoading()) return;

    const int target = mini_acid_.targetPageIndex();
    if (target < 0 || target >= kMaxPages) {
        mini_acid_.setTargetPage(-1);
        mini_acid_.setPageLoading(false);
        showToast("Invalid pattern page", 1500);
        return;
    }

    enum class PageSwitchResult {
        Switched,
        Created,
        SaveCurrentFailed,
        LoadTargetFailed,
        CreateTargetFailed,
        RollbackFailed,
    };

    PageSwitchResult result = PageSwitchResult::LoadTargetFailed;
    const int current = mini_acid_.currentPageIndex();

    withAudioGuard([&]() {
#if defined(ESP32) || defined(ESP_PLATFORM)
        Scene& scene = mini_acid_.sceneManager().currentScene();
        if (!PatternPagingService::savePage(current, scene)) {
            result = PageSwitchResult::SaveCurrentFailed;
        } else if (PatternPagingService::pageExists(target)) {
            if (PatternPagingService::loadPage(target, scene)) {
                mini_acid_.setCurrentPage(target);
                result = PageSwitchResult::Switched;
            } else {
                result = PageSwitchResult::LoadTargetFailed;
            }
        } else {
            PatternPagingService::initializeEmptyPage(scene);
            if (PatternPagingService::savePage(target, scene)) {
                mini_acid_.setCurrentPage(target);
                result = PageSwitchResult::Created;
            } else if (PatternPagingService::loadPage(current, scene)) {
                result = PageSwitchResult::CreateTargetFailed;
            } else {
                result = PageSwitchResult::RollbackFailed;
            }
        }
#else
        mini_acid_.setCurrentPage(target);
        result = PageSwitchResult::Switched;
#endif
        mini_acid_.setTargetPage(-1);
        mini_acid_.setPageLoading(false);
    });

    char message[32];
    switch (result) {
        case PageSwitchResult::Switched:
            std::snprintf(message, sizeof(message), "Pattern Page %d", target + 1);
            showToast(message, 900);
            break;
        case PageSwitchResult::Created:
            std::snprintf(message, sizeof(message), "New Pattern Page %d", target + 1);
            showToast(message, 1200);
            break;
        case PageSwitchResult::SaveCurrentFailed:
            showToast("Page save failed", 1800);
            break;
        case PageSwitchResult::LoadTargetFailed:
            showToast("Page corrupt/unreadable", 1800);
            break;
        case PageSwitchResult::CreateTargetFailed:
            showToast("New page save failed", 1800);
            break;
        case PageSwitchResult::RollbackFailed:
            showToast("PAGE ROLLBACK FAILED", 2500);
            break;
    }
}
'''
    text = text[:start] + replacement
    path.write_text(text, encoding="utf-8")


def patch_scene_manager() -> None:
    path = Path("scenes.cpp")
    text = path.read_text(encoding="utf-8")

    text = replace_function(
        text,
        "void serializeDrumPattern(const DrumPattern& pattern, ArduinoJson::JsonObject obj) {",
        "void serializeAutomationLane(",
        r'''void serializeDrumPattern(const DrumPattern& pattern, ArduinoJson::JsonObject obj) {
  ArduinoJson::JsonArray hit = obj["hit"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray accent = obj["accent"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray velocity = obj["vel"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray timing = obj["tim"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray fx = obj["fx"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray fxp = obj["fxp"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray probability = obj["prb"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    hit.add(pattern.steps[i].hit);
    accent.add(pattern.steps[i].accent);
    velocity.add(pattern.steps[i].velocity);
    timing.add(pattern.steps[i].timing);
    fx.add(pattern.steps[i].fx);
    fxp.add(pattern.steps[i].fxParam);
    probability.add(pattern.steps[i].probability);
  }
}''',
    )

    text = replace_function(
        text,
        "bool deserializeDrumPattern(ArduinoJson::JsonVariantConst value, DrumPattern& pattern) {",
        "bool deserializeAutomationLane(",
        r'''bool deserializeDrumPattern(ArduinoJson::JsonVariantConst value, DrumPattern& pattern) {
  ArduinoJson::JsonObjectConst obj = value.as<ArduinoJson::JsonObjectConst>();
  if (obj.isNull()) return false;

  ArduinoJson::JsonArrayConst hit = obj["hit"].as<ArduinoJson::JsonArrayConst>();
  ArduinoJson::JsonArrayConst accent = obj["accent"].as<ArduinoJson::JsonArrayConst>();
  if (hit.isNull() || accent.isNull()) return false;

  bool hits[DrumPattern::kSteps];
  bool accents[DrumPattern::kSteps];
  if (!deserializeBoolArray(hit, hits, DrumPattern::kSteps) ||
      !deserializeBoolArray(accent, accents, DrumPattern::kSteps)) {
    return false;
  }

  auto readOptionalInts = [&](const char* key, int* output, int defaultValue) {
    for (int i = 0; i < DrumPattern::kSteps; ++i) output[i] = defaultValue;
    ArduinoJson::JsonArrayConst array = obj[key].as<ArduinoJson::JsonArrayConst>();
    if (array.isNull()) return true;
    if (static_cast<int>(array.size()) != DrumPattern::kSteps) return false;
    int index = 0;
    for (ArduinoJson::JsonVariantConst item : array) {
      if (!item.is<int>()) return false;
      output[index++] = item.as<int>();
    }
    return true;
  };

  int velocities[DrumPattern::kSteps];
  int timings[DrumPattern::kSteps];
  int effects[DrumPattern::kSteps];
  int effectParams[DrumPattern::kSteps];
  int probabilities[DrumPattern::kSteps];
  if (!readOptionalInts("vel", velocities, 100) ||
      !readOptionalInts("tim", timings, 0) ||
      !readOptionalInts("fx", effects, 0) ||
      !readOptionalInts("fxp", effectParams, 0) ||
      !readOptionalInts("prb", probabilities, 100)) {
    return false;
  }

  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    int velocity = velocities[i];
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;
    int timing = timings[i];
    if (timing < -23) timing = -23;
    if (timing > 23) timing = 23;
    int probability = probabilities[i];
    if (probability < 0) probability = 0;
    if (probability > 100) probability = 100;

    pattern.steps[i].hit = hits[i];
    pattern.steps[i].accent = accents[i];
    pattern.steps[i].velocity = static_cast<uint8_t>(velocity);
    pattern.steps[i].timing = static_cast<int8_t>(timing);
    pattern.steps[i].fx = static_cast<uint8_t>(effects[i]);
    pattern.steps[i].fxParam = static_cast<uint8_t>(effectParams[i]);
    pattern.steps[i].probability = static_cast<uint8_t>(probability);
  }
  return true;
}''',
    )

    text = replace_function(
        text,
        "void serializeSynthPattern(const SynthPattern& pattern, ArduinoJson::JsonArray steps) {",
        "void serializeSynthBank(",
        r'''void serializeSynthPattern(const SynthPattern& pattern, ArduinoJson::JsonArray steps) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    ArduinoJson::JsonObject step = steps.add<ArduinoJson::JsonObject>();
    step["note"] = pattern.steps[i].note;
    step["slide"] = pattern.steps[i].slide;
    step["accent"] = pattern.steps[i].accent;
    step["ghost"] = pattern.steps[i].ghost;
    step["vel"] = pattern.steps[i].velocity;
    step["tim"] = pattern.steps[i].timing;
    step["fx"] = pattern.steps[i].fx;
    step["fxp"] = pattern.steps[i].fxParam;
    step["prb"] = pattern.steps[i].probability;
  }
}''',
    )

    text = replace_function(
        text,
        "bool deserializeSynthPattern(ArduinoJson::JsonVariantConst value, SynthPattern& pattern) {",
        "bool deserializeSynthBank(",
        r'''bool deserializeSynthPattern(ArduinoJson::JsonVariantConst value, SynthPattern& pattern) {
  ArduinoJson::JsonArrayConst steps = value.as<ArduinoJson::JsonArrayConst>();
  if (steps.isNull() || static_cast<int>(steps.size()) != SynthPattern::kSteps) {
    return false;
  }

  int index = 0;
  for (ArduinoJson::JsonVariantConst stepValue : steps) {
    ArduinoJson::JsonObjectConst obj = stepValue.as<ArduinoJson::JsonObjectConst>();
    if (obj.isNull() || !obj["note"].is<int>() ||
        !obj["slide"].is<bool>() || !obj["accent"].is<bool>()) {
      return false;
    }

    int note = obj["note"].as<int>();
    if (note < -2) note = -2;
    if (note > 127) note = 127;
    int velocity = valueToInt(obj["vel"], 100);
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;
    int timing = valueToInt(obj["tim"], 0);
    if (timing < -23) timing = -23;
    if (timing > 23) timing = 23;
    int probability = valueToInt(obj["prb"], 100);
    if (probability < 0) probability = 0;
    if (probability > 100) probability = 100;

    SynthStep& step = pattern.steps[index++];
    step.note = static_cast<int8_t>(note);
    step.slide = obj["slide"].as<bool>();
    step.accent = obj["accent"].as<bool>();
    step.ghost = obj["ghost"].is<bool>() ? obj["ghost"].as<bool>() : false;
    step.velocity = static_cast<uint8_t>(velocity);
    step.timing = static_cast<int8_t>(timing);
    step.fx = static_cast<uint8_t>(valueToInt(obj["fx"], 0));
    step.fxParam = static_cast<uint8_t>(valueToInt(obj["fxp"], 0));
    step.probability = static_cast<uint8_t>(probability);
  }
  return true;
}''',
    )

    text = replace_once(
        text,
        r'''void SceneManager::setPage(int pageIndex) {
  if (pageIndex < 0) return;
  if (pageIndex == currentPageIndex_) return;
  
  saveCurrentPage();
  currentPageIndex_ = pageIndex;
  loadCurrentPage();
}
''',
        r'''void SceneManager::setPage(int pageIndex) {
  if (pageIndex < 0 || pageIndex >= kMaxPages) return;
  if (pageIndex == currentPageIndex_ || !scene_) return;

  const int previousPage = currentPageIndex_;
  if (!PatternPagingService::savePage(previousPage, *scene_)) return;

  if (PatternPagingService::pageExists(pageIndex)) {
    if (!PatternPagingService::loadPage(pageIndex, *scene_)) return;
  } else {
    PatternPagingService::initializeEmptyPage(*scene_);
    if (!PatternPagingService::savePage(pageIndex, *scene_)) {
      PatternPagingService::loadPage(previousPage, *scene_);
      return;
    }
  }
  currentPageIndex_ = pageIndex;
}
''',
        "SceneManager page switch",
    )

    text = replace_once(
        text,
        r'''void SceneManager::setTrackVolume(int voiceIdx, float volume) {
  if (voiceIdx >= 0 && voiceIdx < (int)VoiceId::Count) {
    scene_->trackVolumes[voiceIdx] = volume;
  }
}
''',
        r'''void SceneManager::setTrackVolume(int voiceIdx, float volume) {
  if (voiceIdx < 0 || voiceIdx >= static_cast<int>(VoiceId::Count)) return;
  if (volume < 0.0f) volume = 0.0f;
  if (volume > 1.5f) volume = 1.5f;
  scene_->trackVolumes[voiceIdx] = volume;
}
''',
        "track volume clamp",
    )

    path.write_text(text, encoding="utf-8")


def patch_scene_save_result() -> None:
    header_path = Path("src/dsp/miniacid_engine.h")
    header = header_path.read_text(encoding="utf-8")
    header = replace_once(
        header,
        "  void saveSceneToStorage();\n",
        "  bool saveSceneToStorage();\n",
        "save declaration",
    )
    header_path.write_text(header, encoding="utf-8")

    source_path = Path("src/dsp/miniacid_engine.cpp")
    source = source_path.read_text(encoding="utf-8")
    source = replace_once(
        source,
        r'''void MiniAcid::saveSceneToStorage() {
  if (!sceneStorage_) return;
  syncSceneStateToManager();
  sceneStorage_->writeScene(sceneManager_);
}
''',
        r'''bool MiniAcid::saveSceneToStorage() {
  if (!sceneStorage_) return false;
  syncSceneStateToManager();
  return sceneStorage_->writeScene(sceneManager_);
}
''',
        "save storage result",
    )
    source = replace_once(
        source,
        r'''bool MiniAcid::saveSceneAs(const std::string& name) {
  if (!sceneStorage_) return false;
  sceneStorage_->setCurrentSceneName(name);
  saveSceneToStorage();
  return true;
}
''',
        r'''bool MiniAcid::saveSceneAs(const std::string& name) {
  if (!sceneStorage_) return false;
  if (!sceneStorage_->setCurrentSceneName(name)) return false;
  return saveSceneToStorage();
}
''',
        "save scene result",
    )
    source_path.write_text(source, encoding="utf-8")


def patch_audio_runtime() -> None:
    path = Path("miniacid.ino")
    text = path.read_text(encoding="utf-8")

    text = text.replace(
        "static uint32_t warmupBlocks = 32; // ~90ms at 44.1kHz/128 for hardware stability",
        "static uint32_t warmupBlocks = 32; // ~743ms at 22.05kHz/512 for codec/DMA stability",
        1,
    )

    start = text.index("    // Update performance stats\n")
    end = text.index("    if (g_audioRecorder) {", start)
    stats = r'''    // Publish one coherent cross-core telemetry snapshot.
    if (g_miniAcid) {
      auto& stats = g_miniAcid->perfStats;
      constexpr uint32_t idealPeriodUs =
          (1000000UL * kBlockFrames) / kSampleRate;
      const uint32_t previousCallback =
          stats.lastCallbackMicros.load(std::memory_order_relaxed);
      uint32_t actualPeriodUs = previousCallback > 0
          ? now - previousCallback
          : idealPeriodUs;
      if (actualPeriodUs == 0) actualPeriodUs = idealPeriodUs;

      const float idealCpu =
          static_cast<float>(dsp_time) * 100.0f /
          static_cast<float>(idealPeriodUs);
      const float actualCpu =
          static_cast<float>(dsp_time) * 100.0f /
          static_cast<float>(actualPeriodUs);

      stats.beginWrite();
      stats.cpuAudioPctIdeal.store(idealCpu, std::memory_order_relaxed);
      stats.cpuAudioPctActual.store(actualCpu, std::memory_order_relaxed);
      stats.dspTimeUs.store(dsp_time, std::memory_order_relaxed);
      stats.lastCallbackMicros.store(now, std::memory_order_relaxed);
      const float previousPeak =
          stats.cpuAudioPeakPct.load(std::memory_order_relaxed);
      if (idealCpu > previousPeak) {
        stats.cpuAudioPeakPct.store(idealCpu, std::memory_order_relaxed);
      }
      stats.endWrite();
    }

'''
    text = text[:start] + stats + text[end:]

    old_i2s = r'''    // Write to I2S
    if (!g_audioOut.writeMono16(g_audioBuffer, kBlockFrames)) {
      static uint32_t lastErrorLog = 0;
      if (millis() - lastErrorLog > 1000) {
        Serial.println("[I2S] Write Timeout / Error");
        lastErrorLog = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
'''
    new_i2s = r'''    // Write to I2S. Failed writes are real output underruns and must be
    // visible to diagnostics and the adaptive FX safety path.
    if (!g_audioOut.writeMono16(g_audioBuffer, kBlockFrames)) {
      if (g_miniAcid) {
        g_miniAcid->perfStats.audioUnderruns.fetch_add(
            1, std::memory_order_relaxed);
      }
      static uint32_t lastErrorLog = 0;
      if (millis() - lastErrorLog > 1000) {
        Serial.println("[I2S] Write Timeout / Error");
        lastErrorLog = millis();
      }
      taskYIELD();
    }
'''
    text = replace_once(text, old_i2s, new_i2s, "I2S underrun telemetry")

    text = replace_once(
        text,
        "  const uint8_t es8311_addr = 0x18;\n",
        "  const uint8_t es8311_addr = GroovePuterHardware::kEs8311I2cAddress;\n",
        "codec address profile",
    )
    path.write_text(text, encoding="utf-8")


def restore_normal_workflow() -> None:
    workflow = Path(".github/workflows/core-regressions.yml")
    workflow.write_text(
        """name: Core regressions

on:
  push:
    branches:
      - main
      - agent/fix-core-reliability
  pull_request:

permissions:
  contents: read

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Run core host regressions
        run: bash tests/run_host_tests.sh

  sdl-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install SDL dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y --no-install-recommends \\
            build-essential \\
            libsdl2-dev \\
            libsdl2-gfx-dev

      - name: Build desktop target
        working-directory: platform_sdl
        run: make clean all CXX=g++
""",
        encoding="utf-8",
    )


def main() -> None:
    patch_pattern_paging()
    patch_display_paging()
    patch_scene_manager()
    patch_scene_save_result()
    patch_audio_runtime()
    restore_normal_workflow()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
