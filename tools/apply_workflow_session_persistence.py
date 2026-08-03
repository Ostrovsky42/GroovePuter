#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one anchor, found {count}: {old[:80]!r}")
    write(path, text.replace(old, new, 1))


# Scene storage recovery contract.
replace_once(
    "scene_storage.h",
    "  virtual bool writeSceneAuto(const SceneManager& manager) = 0;\n"
    "  virtual bool readSceneAuto(SceneManager& manager) = 0;\n",
    "  virtual bool writeSceneAuto(const SceneManager& manager) = 0;\n"
    "  // Reads only the recovery snapshot. The caller owns fallback to the main scene.\n"
    "  virtual bool readSceneAuto(SceneManager& manager) = 0;\n"
    "  virtual bool hasSceneAuto() const = 0;\n"
    "  virtual bool clearSceneAuto() = 0;\n",
)

replace_once(
    "scene_storage_cardputer.h",
    "  bool writeSceneAuto(const SceneManager& manager) override;\n"
    "  bool readSceneAuto(SceneManager& manager) override;\n",
    "  bool writeSceneAuto(const SceneManager& manager) override;\n"
    "  bool readSceneAuto(SceneManager& manager) override;\n"
    "  bool hasSceneAuto() const override;\n"
    "  bool clearSceneAuto() override;\n",
)

replace_once(
    "scene_storage_cardputer.cpp",
    "  return readScene(manager);\n}\n\nstd::vector<std::string> SceneStorageCardputer::getAvailableSceneNames() const {",
    "  return false;\n}\n\n"
    "bool SceneStorageCardputer::hasSceneAuto() const {\n"
    "  if (!isInitialized_) return false;\n"
    "  const std::string path = currentAutoScenePath();\n"
    "  return SD.exists(path.c_str()) ||\n"
    "         SD.exists(siblingPath(path, kBackupSuffix).c_str());\n"
    "}\n\n"
    "bool SceneStorageCardputer::clearSceneAuto() {\n"
    "  if (!isInitialized_) return false;\n"
    "  const std::string path = currentAutoScenePath();\n"
    "  const std::string backup = siblingPath(path, kBackupSuffix);\n"
    "  const std::string temp = siblingPath(path, kTempSuffix);\n"
    "  if (SD.exists(path.c_str()) && !SD.remove(path.c_str())) return false;\n"
    "  if (SD.exists(backup.c_str()) && !SD.remove(backup.c_str())) return false;\n"
    "  if (SD.exists(temp.c_str()) && !SD.remove(temp.c_str())) return false;\n"
    "  return !SD.exists(path.c_str()) && !SD.exists(backup.c_str());\n"
    "}\n\n"
    "std::vector<std::string> SceneStorageCardputer::getAvailableSceneNames() const {",
)

replace_once(
    "platform_sdl/scene_storage_sdl.h",
    "  bool writeSceneAuto(const SceneManager& manager) override;\n"
    "  bool readSceneAuto(SceneManager& manager) override;\n",
    "  bool writeSceneAuto(const SceneManager& manager) override;\n"
    "  bool readSceneAuto(SceneManager& manager) override;\n"
    "  bool hasSceneAuto() const override;\n"
    "  bool clearSceneAuto() override;\n",
)
replace_once(
    "platform_sdl/scene_storage_sdl.h",
    "  static constexpr const char* kSceneExtension = \".json\";\n",
    "  static constexpr const char* kSceneExtension = \".json\";\n"
    "  static constexpr const char* kAutoSceneExtension = \".auto.json\";\n",
)
replace_once(
    "platform_sdl/scene_storage_sdl.h",
    "  std::string sceneFilePath() const;\n",
    "  std::string sceneFilePath() const;\n"
    "  std::string autoSceneFilePath() const;\n"
    "  std::string autoSceneKeyForStorage() const;\n",
)

replace_once(
    "platform_sdl/scene_storage_sdl.cpp",
    "EM_JS(int, wasm_read_current_scene_name, (char* out, int maxLen), {",
    "EM_JS(int, wasm_remove_scene, (const char* key), {\n"
    "  const storageKey = UTF8ToString(key);\n"
    "  try {\n"
    "    localStorage.removeItem(storageKey);\n"
    "    return localStorage.getItem(storageKey) === null ? 1 : 0;\n"
    "  } catch (e) {\n"
    "    return 0;\n"
    "  }\n"
    "});\n\n"
    "EM_JS(int, wasm_read_current_scene_name, (char* out, int maxLen), {",
)
replace_once(
    "platform_sdl/scene_storage_sdl.cpp",
    "      if(key.substring(prefix.length + 1) == \"current\") continue;\n"
    "      names.push(key.substring(prefix.length + 1));",
    "      const suffix = key.substring(prefix.length + 1);\n"
    "      if (suffix === 'current' || suffix.endsWith(':auto')) continue;\n"
    "      names.push(suffix);",
)
replace_once(
    "platform_sdl/scene_storage_sdl.cpp",
    "std::string SceneStorageSdl::sceneFilePath() const {\n"
    "  std::string path = normalizeSceneName(currentSceneName_);\n"
    "  path += kSceneExtension;\n"
    "  return path;\n"
    "}\n",
    "std::string SceneStorageSdl::sceneFilePath() const {\n"
    "  std::string path = normalizeSceneName(currentSceneName_);\n"
    "  path += kSceneExtension;\n"
    "  return path;\n"
    "}\n\n"
    "std::string SceneStorageSdl::autoSceneFilePath() const {\n"
    "  return normalizeSceneName(currentSceneName_) + kAutoSceneExtension;\n"
    "}\n\n"
    "std::string SceneStorageSdl::autoSceneKeyForStorage() const {\n"
    "  return sceneKeyForStorage(currentSceneName_) + \":auto\";\n"
    "}\n",
)
replace_once(
    "platform_sdl/scene_storage_sdl.cpp",
    "    if (path.extension() == kSceneExtension) {\n"
    "      names.push_back(path.stem().string());\n"
    "    }",
    "    const std::string filename = path.filename().string();\n"
    "    const size_t autoLen = std::strlen(kAutoSceneExtension);\n"
    "    if (filename.size() >= autoLen &&\n"
    "        filename.compare(filename.size() - autoLen, autoLen,\n"
    "                         kAutoSceneExtension) == 0) {\n"
    "      continue;\n"
    "    }\n"
    "    if (path.extension() == kSceneExtension) {\n"
    "      names.push_back(path.stem().string());\n"
    "    }",
)
replace_once(
    "platform_sdl/scene_storage_sdl.cpp",
    "bool SceneStorageSdl::writeSceneAuto(const SceneManager& manager) {\n"
    "  // For SDL/Desktop, auto-save = regular save (simplified)\n"
    "  return writeScene(manager);\n"
    "}\n\n"
    "bool SceneStorageSdl::readSceneAuto(SceneManager& manager) {\n"
    "  // For SDL/Desktop, auto-load = regular load (simplified)\n"
    "  return readScene(manager);\n"
    "}",
    "bool SceneStorageSdl::writeSceneAuto(const SceneManager& manager) {\n"
    "  std::string serialized;\n"
    "  if (!manager.writeSceneJson(serialized)) return false;\n"
    "#ifdef __EMSCRIPTEN__\n"
    "  const std::string key = autoSceneKeyForStorage();\n"
    "  return wasm_write_scene(key.c_str(), serialized.c_str()) > 0;\n"
    "#else\n"
    "  std::ofstream file(autoSceneFilePath(), std::ios::out | std::ios::trunc);\n"
    "  if (!file.is_open()) return false;\n"
    "  file << serialized;\n"
    "  return file.good();\n"
    "#endif\n"
    "}\n\n"
    "bool SceneStorageSdl::readSceneAuto(SceneManager& manager) {\n"
    "  std::string serialized;\n"
    "#ifdef __EMSCRIPTEN__\n"
    "  const std::string key = autoSceneKeyForStorage();\n"
    "  const int length = wasm_read_scene(key.c_str(), nullptr, 0);\n"
    "  if (length <= 0) return false;\n"
    "  serialized.resize(static_cast<size_t>(length));\n"
    "  const int written = wasm_read_scene(key.c_str(), serialized.data(), length);\n"
    "  if (written <= 0) return false;\n"
    "  serialized.resize(static_cast<size_t>(written));\n"
    "#else\n"
    "  std::ifstream file(autoSceneFilePath(), std::ios::in);\n"
    "  if (!file.is_open()) return false;\n"
    "  serialized.assign(std::istreambuf_iterator<char>(file),\n"
    "                    std::istreambuf_iterator<char>());\n"
    "#endif\n"
    "  return !serialized.empty() && manager.loadScene(serialized);\n"
    "}\n\n"
    "bool SceneStorageSdl::hasSceneAuto() const {\n"
    "#ifdef __EMSCRIPTEN__\n"
    "  const std::string key = autoSceneKeyForStorage();\n"
    "  return wasm_read_scene(key.c_str(), nullptr, 0) > 0;\n"
    "#else\n"
    "  return std::filesystem::exists(autoSceneFilePath());\n"
    "#endif\n"
    "}\n\n"
    "bool SceneStorageSdl::clearSceneAuto() {\n"
    "#ifdef __EMSCRIPTEN__\n"
    "  const std::string key = autoSceneKeyForStorage();\n"
    "  return wasm_remove_scene(key.c_str()) > 0;\n"
    "#else\n"
    "  std::error_code error;\n"
    "  std::filesystem::remove(autoSceneFilePath(), error);\n"
    "  return !error && !std::filesystem::exists(autoSceneFilePath());\n"
    "#endif\n"
    "}",
)

# Engine: device master volume and recovery autosave.
replace_once(
    "src/dsp/miniacid_engine.h",
    "  std::string currentSceneName() const;\n"
    "  std::vector<std::string> availableSceneNames() const;\n",
    "  std::string currentSceneName() const;\n"
    "  std::vector<std::string> availableSceneNames() const;\n"
    "  bool autoSaveSceneRecovery();\n"
    "  bool lastSceneLoadRecoveredAutosave() const {\n"
    "    return lastSceneLoadRecoveredAutosave_;\n"
    "  }\n"
    "  float mainVolume() const;\n"
    "  void setDeviceMasterVolume(float value);\n",
)
replace_once(
    "src/dsp/miniacid_engine.h",
    "  bool testToneEnabled_ = false;\n"
    "  float testTonePhase_ = 0.0f;\n",
    "  bool testToneEnabled_ = false;\n"
    "  float testTonePhase_ = 0.0f;\n"
    "  bool deviceMasterVolumeOverride_ = false;\n"
    "  bool lastSceneLoadRecoveredAutosave_ = false;\n",
)

replace_once(
    "src/dsp/miniacid_engine.cpp",
    "  bool loaded = sceneStorage_->readScene(sceneManager_);\n"
    "  Serial.printf(\"[LoadScene] readScene returned: %s\\n\", loaded ? \"TRUE\" : \"FALSE\");\n"
    "  // String-based fallback REMOVED - causes OOM on DRAM-only devices\n",
    "  bool recoveredAuto = false;\n"
    "  bool loaded = false;\n"
    "  if (sceneStorage_->hasSceneAuto()) {\n"
    "    recoveredAuto = sceneStorage_->readSceneAuto(sceneManager_);\n"
    "    loaded = recoveredAuto;\n"
    "  }\n"
    "  if (!loaded) loaded = sceneStorage_->readScene(sceneManager_);\n"
    "  lastSceneLoadRecoveredAutosave_ = loaded && recoveredAuto;\n"
    "  Serial.printf(\"[LoadScene] loaded=%d recovery=%d\\n\",\n"
    "                loaded ? 1 : 0, recoveredAuto ? 1 : 0);\n"
    "  // String-based fallback REMOVED - causes OOM on DRAM-only devices\n",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    "    sceneStorage_->setCurrentSceneName(previousName);\n"
    "    return false;\n"
    "  }\n"
    "  Serial.println(\"[LoadScene] Applying scene state...\");",
    "    sceneStorage_->setCurrentSceneName(previousName);\n"
    "    lastSceneLoadRecoveredAutosave_ = false;\n"
    "    return false;\n"
    "  }\n"
    "  Serial.println(\"[LoadScene] Applying scene state...\");",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    "void MiniAcid::loadSceneFromStorage() {\n"
    "  if (sceneStorage_) {\n"
    "    if (sceneStorage_->readScene(sceneManager_)) return;\n"
    "    // String-based fallback REMOVED - it causes OOM on DRAM-only devices\n"
    "    // If streaming parse fails, load default scene\n"
    "    LOG_PRINTLN(\"  - loadSceneFromStorage: Streaming parse failed, loading default scene\");\n"
    "  }\n"
    "  sceneManager_.loadDefaultScene();\n"
    "}\n\n"
    "bool MiniAcid::saveSceneToStorage() {\n"
    "  if (!sceneStorage_) return false;\n"
    "  syncSceneStateToManager();\n"
    "  return sceneStorage_->writeScene(sceneManager_);\n"
    "}\n",
    "void MiniAcid::loadSceneFromStorage() {\n"
    "  lastSceneLoadRecoveredAutosave_ = false;\n"
    "  if (sceneStorage_) {\n"
    "    if (sceneStorage_->hasSceneAuto() &&\n"
    "        sceneStorage_->readSceneAuto(sceneManager_)) {\n"
    "      lastSceneLoadRecoveredAutosave_ = true;\n"
    "      LOG_PRINTLN(\"  - loadSceneFromStorage: recovered autosave\");\n"
    "      return;\n"
    "    }\n"
    "    if (sceneStorage_->readScene(sceneManager_)) return;\n"
    "    LOG_PRINTLN(\"  - loadSceneFromStorage: Streaming parse failed, loading default scene\");\n"
    "  }\n"
    "  sceneManager_.loadDefaultScene();\n"
    "}\n\n"
    "bool MiniAcid::saveSceneToStorage() {\n"
    "  if (!sceneStorage_) return false;\n"
    "  syncSceneStateToManager();\n"
    "  if (!sceneStorage_->writeScene(sceneManager_)) return false;\n"
    "  if (!sceneStorage_->clearSceneAuto()) {\n"
    "    Serial.println(\"[SceneSave] main saved but recovery cleanup failed\");\n"
    "    return false;\n"
    "  }\n"
    "  lastSceneLoadRecoveredAutosave_ = false;\n"
    "  return true;\n"
    "}\n\n"
    "bool MiniAcid::autoSaveSceneRecovery() {\n"
    "  if (!sceneStorage_ || playing) return false;\n"
    "  syncSceneStateToManager();\n"
    "  return sceneStorage_->writeSceneAuto(sceneManager_);\n"
    "}\n\n"
    "float MiniAcid::mainVolume() const {\n"
    "  return params[static_cast<int>(MiniAcidParamId::MainVolume)].value();\n"
    "}\n\n"
    "void MiniAcid::setDeviceMasterVolume(float value) {\n"
    "  params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(value);\n"
    "  deviceMasterVolumeOverride_ = true;\n"
    "}\n",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    "  // Load master volume from scene\n"
    "  float sceneVolume = sceneManager_.currentScene().masterVolume;\n"
    "  params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(sceneVolume);\n",
    "  // Scene volume remains codec-compatible, but a device-session override\n"
    "  // wins after boot so loading another project cannot change speaker level.\n"
    "  if (!deviceMasterVolumeOverride_) {\n"
    "    const float sceneVolume = sceneManager_.currentScene().masterVolume;\n"
    "    params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(sceneVolume);\n"
    "  }\n",
)

# Project load/save baseline and volume semantics.
replace_once(
    "src/ui/pages/project_page.cpp",
    "  if (loaded) {\n"
    "    GroovePuterState::markSceneLoadSucceeded();\n"
    "    closeDialog();\n",
    "  if (loaded) {\n"
    "    if (mini_acid_.lastSceneLoadRecoveredAutosave()) {\n"
    "      GroovePuterState::markSceneMutated();\n"
    "      UI::showToast(\"Recovered unsaved project\", 1800);\n"
    "    } else {\n"
    "      GroovePuterState::markSceneLoadSucceeded();\n"
    "    }\n"
    "    closeDialog();\n",
)
replace_once(
    "src/ui/pages/project_page.cpp",
    "                mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, right ? 1 : -1);\n"
    "                GroovePuterState::markSceneMutated();\n"
    "                return true;\n",
    "                mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, right ? 1 : -1);\n"
    "                return true;\n",
)

# Display session and autosave service.
replace_once(
    "src/ui/miniacid_display.h",
    "#include \"src/platform/cardputer_midi_settings_session.h\"\n",
    "#include \"src/platform/cardputer_midi_settings_session.h\"\n"
    "#include \"src/state/ui_session_state.h\"\n",
)
replace_once(
    "src/ui/miniacid_display.h",
    "  void handlePaging_();\n",
    "  void handlePaging_();\n"
    "  void captureUiSession_();\n"
    "  void scheduleUiSessionSave_();\n"
    "  void servicePersistence_();\n",
)
replace_once(
    "src/ui/miniacid_display.h",
    "  bool visual_style_initialized_ = false;\n",
    "  bool visual_style_initialized_ = false;\n"
    "  GroovePuterState::UiSessionState ui_session_{};\n"
    "  bool ui_session_loaded_ = false;\n"
    "  bool ui_session_save_pending_ = false;\n"
    "  unsigned long ui_session_save_due_ms_ = 0;\n"
    "  uint32_t observed_scene_revision_ = 0;\n"
    "  bool recovery_save_pending_ = false;\n"
    "  unsigned long recovery_save_due_ms_ = 0;\n",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "#include \"src/state/scene_revision.h\"\n",
    "#include \"src/state/scene_revision.h\"\n"
    "#include \"src/platform/cardputer_ui_session.h\"\n",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "    splash_start_ms_ = millis();\n"
    "    splash_active_ = true;\n\n"
    "    LOG_DEBUG_UI(\"Initializing skin and pages...\");",
    "    splash_start_ms_ = millis();\n"
    "    splash_active_ = true;\n\n"
    "    ui_session_ = GroovePuterState::defaultUiSessionState();\n"
    "    ui_session_loaded_ =\n"
    "        GroovePuterPlatform::loadCardputerUiSession(ui_session_);\n"
    "    if (!ui_session_loaded_) {\n"
    "        ui_session_.masterVolumePermille =\n"
    "            GroovePuterState::masterVolumeToPermille(mini_acid_.mainVolume());\n"
    "    }\n"
    "    GroovePuterState::sanitizeUiSessionState(ui_session_);\n"
    "    page_index_ = ui_session_.activePage;\n"
    "    previous_page_index_ = page_index_;\n"
    "    active_workspace_ = WorkflowPages::workspaceForPage(page_index_);\n"
    "    UI::currentStyle = static_cast<VisualStyle>(ui_session_.visualStyle);\n"
    "    UI::waveformOverlay.enabled = ui_session_.waveformOverlayEnabled != 0;\n"
    "    if (mini_acid_.lastSceneLoadRecoveredAutosave()) {\n"
    "        GroovePuterState::markSceneMutated();\n"
    "    }\n"
    "    observed_scene_revision_ =\n"
    "        GroovePuterState::sceneRevisionSnapshot().currentRevision;\n\n"
    "    LOG_DEBUG_UI(\"Initializing skin and pages...\");",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "void MiniAcidDisplay::setAudioGuard(AudioGuard guard) {\n"
    "    audio_guard_ = guard;\n"
    "}\n",
    "void MiniAcidDisplay::setAudioGuard(AudioGuard guard) {\n"
    "    audio_guard_ = guard;\n"
    "    const float persistedVolume =\n"
    "        GroovePuterState::masterVolumeFromPermille(\n"
    "            ui_session_.masterVolumePermille);\n"
    "    withAudioGuard([&]() {\n"
    "        mini_acid_.setDeviceMasterVolume(persistedVolume);\n"
    "    });\n"
    "}\n",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "void MiniAcidDisplay::update() {\n"
    "    syncVisualStyle_();\n",
    "void MiniAcidDisplay::update() {\n"
    "    servicePersistence_();\n"
    "    syncVisualStyle_();\n",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "void MiniAcidDisplay::nextPage() {\n"
    "    const Workspace nextWorkspace = WorkflowPages::nextWorkspace(active_workspace_, 1);\n"
    "    const int next = WorkflowPages::pageForWorkspace(nextWorkspace);\n"
    "    LOG_DEBUG_UI(\"nextWorkspace: %s -> %s\",\n"
    "                 WorkflowPages::workspaceName(active_workspace_),\n"
    "                 WorkflowPages::workspaceName(nextWorkspace));\n"
    "    transitionToPage_(next);\n"
    "}\n\n"
    "void MiniAcidDisplay::previousPage() {\n"
    "    const Workspace previousWorkspace = WorkflowPages::nextWorkspace(active_workspace_, -1);\n"
    "    const int previous = WorkflowPages::pageForWorkspace(previousWorkspace);\n"
    "    LOG_DEBUG_UI(\"previousWorkspace: %s -> %s\",\n"
    "                 WorkflowPages::workspaceName(active_workspace_),\n"
    "                 WorkflowPages::workspaceName(previousWorkspace));\n"
    "    transitionToPage_(previous);\n"
    "}\n",
    "void MiniAcidDisplay::nextPage() {\n"
    "    const bool workflowModifier =\n"
    "        WorkflowPages::hardwareWorkflowModifierHeld();\n"
    "    transitionToPage_(GroovePuterState::workflowNavigationTarget(\n"
    "        ui_session_, page_index_, 1, workflowModifier));\n"
    "}\n\n"
    "void MiniAcidDisplay::previousPage() {\n"
    "    const bool workflowModifier =\n"
    "        WorkflowPages::hardwareWorkflowModifierHeld();\n"
    "    transitionToPage_(GroovePuterState::workflowNavigationTarget(\n"
    "        ui_session_, page_index_, -1, workflowModifier));\n"
    "}\n",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "    if (WorkflowPages::isWorkspacePage(index)) {\n"
    "        active_workspace_ = WorkflowPages::workspaceForPage(index);\n"
    "    }\n",
    "    if (WorkflowPages::isWorkspacePage(index)) {\n"
    "        active_workspace_ = WorkflowPages::workspaceForPage(index);\n"
    "    }\n"
    "    GroovePuterState::rememberWorkflowPage(ui_session_, index);\n"
    "    scheduleUiSessionSave_();\n",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    "            goToPage(WorkflowPages::pageForMode(\n"
    "                WorkflowPages::nextMode(current, direction)));\n",
    "            goToPage(GroovePuterState::rememberedWorkflowPage(\n"
    "                ui_session_, WorkflowPages::nextMode(current, direction)));\n",
)

# Add persistence methods before syncVisualStyle_.
replace_once(
    "src/ui/miniacid_display.cpp",
    "void MiniAcidDisplay::syncVisualStyle_() {",
    "void MiniAcidDisplay::captureUiSession_() {\n"
    "    GroovePuterState::UiSessionState next = ui_session_;\n"
    "    GroovePuterState::rememberWorkflowPage(next, page_index_);\n"
    "    next.visualStyle = static_cast<uint8_t>(UI::currentStyle);\n"
    "    next.waveformOverlayEnabled = UI::waveformOverlay.enabled ? 1 : 0;\n"
    "    next.masterVolumePermille =\n"
    "        GroovePuterState::masterVolumeToPermille(mini_acid_.mainVolume());\n"
    "    GroovePuterState::sanitizeUiSessionState(next);\n"
    "    if (next == ui_session_) return;\n"
    "    ui_session_ = next;\n"
    "    scheduleUiSessionSave_();\n"
    "}\n\n"
    "void MiniAcidDisplay::scheduleUiSessionSave_() {\n"
    "    ui_session_save_pending_ = true;\n"
    "    ui_session_save_due_ms_ = millis() + 1000;\n"
    "}\n\n"
    "void MiniAcidDisplay::servicePersistence_() {\n"
    "    const unsigned long now = millis();\n"
    "    const auto due = [now](unsigned long deadline) {\n"
    "        return static_cast<int32_t>(now - deadline) >= 0;\n"
    "    };\n\n"
    "    captureUiSession_();\n"
    "    if (ui_session_save_pending_ && !mini_acid_.isPlaying() &&\n"
    "        due(ui_session_save_due_ms_)) {\n"
    "        if (GroovePuterPlatform::saveCardputerUiSession(ui_session_)) {\n"
    "            ui_session_save_pending_ = false;\n"
    "        } else {\n"
    "            ui_session_save_due_ms_ = now + 5000;\n"
    "        }\n"
    "    }\n\n"
    "    const GroovePuterState::SceneRevisionState revision =\n"
    "        GroovePuterState::sceneRevisionSnapshot();\n"
    "    if (!revision.dirty()) {\n"
    "        observed_scene_revision_ = revision.currentRevision;\n"
    "        recovery_save_pending_ = false;\n"
    "        return;\n"
    "    }\n"
    "    if (revision.currentRevision != observed_scene_revision_) {\n"
    "        observed_scene_revision_ = revision.currentRevision;\n"
    "        recovery_save_pending_ = true;\n"
    "        recovery_save_due_ms_ = now + 3000;\n"
    "    }\n"
    "    if (!recovery_save_pending_ || mini_acid_.isPlaying() ||\n"
    "        !due(recovery_save_due_ms_)) {\n"
    "        return;\n"
    "    }\n\n"
    "    bool saved = false;\n"
    "    withAudioGuard([&]() { saved = mini_acid_.autoSaveSceneRecovery(); });\n"
    "    if (saved) {\n"
    "        recovery_save_pending_ = false;\n"
    "        Serial.printf(\"[AUTOSAVE] recovery revision=%u\\n\",\n"
    "                      static_cast<unsigned>(observed_scene_revision_));\n"
    "    } else {\n"
    "        recovery_save_due_ms_ = now + 5000;\n"
    "        Serial.println(\"[AUTOSAVE] recovery write failed; retry deferred\");\n"
    "    }\n"
    "}\n\n"
    "void MiniAcidDisplay::syncVisualStyle_() {",
)

# SDL links the no-op platform implementation.
replace_once(
    "platform_sdl/Makefile",
    "\t../src/ui/midi_file_manager.cpp \\\n",
    "\t../src/ui/midi_file_manager.cpp \\\n"
    "\t../src/platform/cardputer_ui_session.cpp \\\n",
)

# Host suite.
replace_once(
    "tests/run_host_tests.sh",
    "python3 \"${ROOT_DIR}/tests/test_scene_revision_source_regressions.py\"\n",
    "python3 \"${ROOT_DIR}/tests/test_scene_revision_source_regressions.py\"\n"
    "python3 \"${ROOT_DIR}/tests/test_ui_session_source_regressions.py\"\n",
)
write(
    "tests/run_host_tests.sh",
    read("tests/run_host_tests.sh") +
    "\n\n\"${CXX}\" \\\n"
    "  -std=c++17 \\\n"
    "  -Wall \\\n"
    "  -Wextra \\\n"
    "  -Werror \\\n"
    "  -I\"${ROOT_DIR}\" \\\n"
    "  \"${ROOT_DIR}/tests/test_ui_session_state.cpp\" \\\n"
    "  -o \"${BUILD_DIR}/test_ui_session_state\"\n\n"
    "\"${BUILD_DIR}/test_ui_session_state\"\n",
)

# Replace obsolete first-page source expectations with remembered-page ownership.
theme_test = read("tests/test_theme_selection_source_regressions.py")
old_start = theme_test.index("    require(\"inline Workspace nextWorkspace(Workspace workspace,\"")
old_end = theme_test.index("\n    page_dispatch =", old_start)
new_block = '''    session = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
    require("workflowNavigationTarget" in session and
            "rememberedWorkflowPage" in session,
            "workflow changes must resolve through per-workflow page memory")
    require("WorkflowPages::hardwareWorkflowModifierHeld()" in display and
            display.count("workflowNavigationTarget") >= 2,
            "plain/Fn brackets must use the remembered workflow navigation model")
    require("rememberedWorkflowPage" in display and
            "WorkflowPages::nextMode(current, direction)" in display,
            "Fn+Tab must restore the remembered page of the adjacent workflow")
    require("pageForMode(\\n                WorkflowPages::nextMode" not in display,
            "workflow switches must not reset to the first page")
'''
theme_test = theme_test[:old_start] + new_block + theme_test[old_end:]
write("tests/test_theme_selection_source_regressions.py", theme_test)

write(
    "tests/test_ui_session_source_regressions.py",
    '''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    state = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
    platform = (ROOT / "src/platform/cardputer_ui_session.cpp").read_text(encoding="utf-8")
    display_h = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    storage = (ROOT / "scene_storage.h").read_text(encoding="utf-8")
    card_storage = (ROOT / "scene_storage_cardputer.cpp").read_text(encoding="utf-8")
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

    require("static_assert(sizeof(UiSessionState) <= 12" in state,
            "UI session RAM contract is missing")
    require("lastPageByWorkflow" in state and "workflowNavigationTarget" in state,
            "per-workflow page memory model is missing")
    require("Preferences" in platform and 'kSessionNamespace = "gp-session"' in platform,
            "Cardputer UI session must use a bounded NVS namespace")
    require("checksumRecord" in platform and "sanitizeUiSessionState" in platform,
            "NVS session record needs integrity and range validation")

    require("UiSessionState ui_session_" in display_h and
            "rememberWorkflowPage(ui_session_, index)" in display,
            "display transitions must update workflow page memory")
    require(display.count("workflowNavigationTarget") >= 2 and
            "rememberedWorkflowPage" in display,
            "workflow navigation must restore remembered pages")
    require("ui_session_save_due_ms_ = millis() + 1000" in display and
            "!mini_acid_.isPlaying()" in display,
            "NVS writes must be debounced and deferred until transport stops")
    require("masterVolumePermille" in state and
            "setDeviceMasterVolume" in display,
            "master volume must restore as device session state")

    require("autoSaveSceneRecovery" in engine_h and
            "writeSceneAuto(sceneManager_)" in engine,
            "dirty project recovery autosave is not wired")
    require("if (!sceneStorage_ || playing) return false" in engine,
            "recovery autosave must reject playback-time writes")
    require("hasSceneAuto" in storage and "clearSceneAuto" in storage,
            "storage recovery lifecycle contract is incomplete")
    require("return false;\n}\n\nbool SceneStorageCardputer::hasSceneAuto" in card_storage,
            "recovery reader must not silently fall back to the main project")
    require("sceneStorage_->clearSceneAuto()" in engine,
            "manual saves must clear stale recovery state")
    require("lastSceneLoadRecoveredAutosave" in project and
            '"Recovered unsaved project"' in project,
            "recovered projects must remain visibly dirty")

    volume_pos = project.index("MiniAcidParamId::MainVolume")
    volume_block = project[volume_pos:volume_pos + 260]
    require("markSceneMutated" not in volume_block,
            "device master volume must not dirty the musical project")


if __name__ == "__main__":
    main()
''',
)

# Documentation.
write(
    "docs/stages/WORKFLOW_SESSION_PERSISTENCE_STAGE.md",
    '''# Workflow Session and Recovery Persistence

## Purpose

Keep navigation and device preferences stable across workflow switches and
reboots, while preserving the explicit distinction between a manually saved
project and a temporary recovery snapshot.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable
- microSD card containing at least two projects

## Wiring

No external wiring is required. PORT.A is unused. Existing Cardputer ADV bus
assumptions remain GPIO2 SDA / GPIO1 SCL.

## Build / Flash

```bash
bash tests/run_host_tests.sh
cd platform_sdl && make clean all CXX=g++
cd ..
bash scripts/build.sh --warnings all
```

Flash with the repository's normal Cardputer ADV procedure.

## Expected behavior

- Each of PERFORM, GENERATE, HUB, SONG and SETTINGS remembers its last page.
- Fn+Tab and Fn+[ / ] return to that page instead of the workflow's first page.
- Plain [ / ] still moves locally inside the current workflow.
- Reboot restores the active page, per-workflow pages, CYBER/CARBON/AMBER theme,
  waveform overlay state and master volume.
- Master volume is a device preference and loading another project does not
  change it.
- The selected project name remains stored transactionally on SD.
- Dirty project edits are recovery-saved after three idle seconds, but only
  while transport is stopped.
- Recovery autosave does not clear the `*` marker.
- Manual Save writes the main project and removes the recovery file.
- Rebooting after an unsaved edit loads recovery and keeps the project dirty.

NVS stores only the compact UI/device session. Project data and recovery remain
on the microSD card.

## Troubleshooting

### Workflow returns to its first page

Verify `tests/test_ui_session_state.cpp` passes and that all page transitions go
through `MiniAcidDisplay::transitionToPage_()`.

### Theme or volume resets after reboot

Stop playback, leave the device running for at least one second, then reboot.
NVS writes are deliberately deferred while transport is active.

### Unsaved edit is not recovered

Stop playback and allow at least three seconds after the last persistent edit.
Check Serial for `[AUTOSAVE] recovery revision=` and verify the microSD card is
mounted and writable.

### Project always appears dirty after manual Save

Check that the main scene write and recovery cleanup both succeeded. A failed
recovery cleanup intentionally keeps Save from establishing a clean baseline.

## Acceptance checklist

- [ ] Leave PERFORM on MIDI Player, switch away and back, and remain on Player.
- [ ] Leave GENERATE on FEEL/TEXTURE and return to FEEL/TEXTURE.
- [ ] Leave HUB on Synth B parameters and return to that page.
- [ ] Leave SETTINGS on Advanced Generator and return to it.
- [ ] Plain brackets still wrap only inside the current workflow.
- [ ] Reboot restores the active page and all remembered workflow pages.
- [ ] Reboot restores theme and waveform overlay state.
- [ ] Reboot restores master volume within one UI step.
- [ ] Loading another project does not change device master volume.
- [ ] Reboot restores the last selected project.
- [ ] An unsaved stopped edit is recovered after reboot and still shows `*`.
- [ ] Playback-time edits do not cause SD/NVS writes until transport stops.
- [ ] Manual Save clears `*` and removes the recovery snapshot.
- [ ] Host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes.
- [ ] No new audio underruns or watchdog resets occur.
''',
)

# Update the earlier navigation stage's now-obsolete first-page statement.
nav_doc = read("docs/stages/UI_WORKFLOW_PAGE_NAVIGATION_STAGE.md")
nav_doc = nav_doc.replace(
    "Fn+[ / ] previous/next workflow, first page",
    "Fn+[ / ] previous/next workflow, remembered page",
)
nav_doc = nav_doc.replace(
    "target workflow opens on its first page",
    "target workflow restores its last opened page",
)
write("docs/stages/UI_WORKFLOW_PAGE_NAVIGATION_STAGE.md", nav_doc)

print("workflow session persistence patch applied")
