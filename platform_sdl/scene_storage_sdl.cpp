#include "scene_storage_sdl.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <cstring>
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif

#include "scenes.h"
#include "../src/audio/pattern_paging.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

EM_JS(int, wasm_read_scene, (const char* key, char* out, int maxLen), {
  const storageKey = UTF8ToString(key);
  try {
    const data = localStorage.getItem(storageKey);
    if (data === null) return -1;
    const len = lengthBytesUTF8(data) + 1;
    if (!out) return len;
    if (len > maxLen) return -2;
    stringToUTF8(data, out, maxLen);
    return len - 1;
  } catch (e) {
    return -3;
  }
});

EM_JS(int, wasm_write_scene, (const char* key, const char* data), {
  const storageKey = UTF8ToString(key);
  try {
    const value = UTF8ToString(data);
    localStorage.setItem(storageKey, value);
    return 1;
  } catch (e) {
    return 0;
  }
});

EM_JS(int, wasm_remove_scene, (const char* key), {
  const storageKey = UTF8ToString(key);
  try {
    localStorage.removeItem(storageKey);
    return localStorage.getItem(storageKey) === null ? 1 : 0;
  } catch (e) {
    return 0;
  }
});

EM_JS(int, wasm_read_current_scene_name, (char* out, int maxLen), {
  const key = 'grooveputer:scene:current';
  try {
    const data = localStorage.getItem(key);
    if (data === null) return -1;
    const len = lengthBytesUTF8(data) + 1;
    if (!out) return len;
    if (len > maxLen) return -2;
    stringToUTF8(data, out, maxLen);
    return len - 1;
  } catch (e) {
    return -3;
  }
});

EM_JS(int, wasm_write_current_scene_name, (const char* name), {
  const key = 'grooveputer:scene:current';
  try {
    const value = UTF8ToString(name);
    localStorage.setItem(key, value);
    return 1;
  } catch (e) {
    return 0;
  }
});

EM_JS(int, wasm_list_scene_names, (char* out, int maxLen), {
  const names = [];
  const prefix = 'grooveputer:scene';
  for (let i = 0; i < localStorage.length; ++i) {
    const key = localStorage.key(i);
    if (!key || !key.startsWith(prefix)) continue;
    if (key === prefix) {
      names.push('grooveputer_scene');
    } else if (key.startsWith(prefix + ':')) {
      const suffix = key.substring(prefix.length + 1);
      if (suffix === 'current' || suffix.endsWith(':auto')) continue;
      names.push(suffix);
    }
  }
  const joined = names.join('\\n');
  const len = lengthBytesUTF8(joined) + 1;
  if (!out) return len;
  if (len > maxLen) return -1;
  stringToUTF8(joined, out, maxLen);
  return names.length;
});
#endif

SceneStorageSdl::SceneStorageSdl() : currentSceneName_(kDefaultSceneName) {}

void SceneStorageSdl::initializeStorage() {
  loadStoredSceneName();
  PatternPagingService::setProjectName(currentSceneName_);
}

std::string SceneStorageSdl::normalizeSceneName(const std::string& name) const {
  std::string cleaned = name;
  if (cleaned.empty()) cleaned = kDefaultSceneName;
  if (cleaned.size() >= std::strlen(kSceneExtension) &&
      cleaned.compare(cleaned.size() - std::strlen(kSceneExtension),
                      std::strlen(kSceneExtension), kSceneExtension) == 0) {
    cleaned.resize(cleaned.size() - std::strlen(kSceneExtension));
  }
  if (cleaned.empty()) cleaned = kDefaultSceneName;
  return cleaned;
}

std::string SceneStorageSdl::sceneFilePath() const {
  std::string path = normalizeSceneName(currentSceneName_);
  path += kSceneExtension;
  return path;
}

std::string SceneStorageSdl::autoSceneFilePath() const {
  return normalizeSceneName(currentSceneName_) + kAutoSceneExtension;
}

std::string SceneStorageSdl::autoSceneKeyForStorage() const {
  return sceneKeyForStorage(currentSceneName_) + ":auto";
}

void SceneStorageSdl::loadStoredSceneName() {
#ifdef __EMSCRIPTEN__
  int length = wasm_read_current_scene_name(nullptr, 0);
  if (length <= 0) return;
  std::string buffer;
  buffer.resize(static_cast<size_t>(length));
  int written = wasm_read_current_scene_name(buffer.data(), length);
  if (written > 0) {
    buffer.resize(static_cast<size_t>(written));
    currentSceneName_ = normalizeSceneName(buffer);
  }
#else
  std::ifstream file(kSceneNameFile, std::ios::in);
  if (!file.is_open()) return;
  std::string storedName;
  std::getline(file, storedName);
  if (!storedName.empty()) currentSceneName_ = normalizeSceneName(storedName);
#endif
}

bool SceneStorageSdl::persistCurrentSceneName() const {
#ifdef __EMSCRIPTEN__
  return wasm_write_current_scene_name(currentSceneName_.c_str()) > 0;
#else
  std::ofstream file(kSceneNameFile, std::ios::out | std::ios::trunc);
  if (!file.is_open()) return false;
  file << currentSceneName_;
  return file.good();
#endif
}

std::string SceneStorageSdl::sceneKeyForStorage(const std::string& name) const {
  static constexpr const char* kLegacyKey = "grooveputer:scene";
  static constexpr const char* kKeyPrefix = "grooveputer:scene:";
  if (name.empty() || name == kDefaultSceneName) return kLegacyKey;
  return std::string(kKeyPrefix) + name;
}

bool SceneStorageSdl::readScene(std::string& out) {
#ifdef __EMSCRIPTEN__
  std::string key = sceneKeyForStorage(currentSceneName_);
  int length = wasm_read_scene(key.c_str(), nullptr, 0);
  if (length <= 0) return false;
  std::string buffer;
  buffer.resize(static_cast<size_t>(length));
  int written = wasm_read_scene(key.c_str(), buffer.data(), length);
  if (written <= 0) return false;
  buffer.resize(static_cast<size_t>(written));
  out = buffer;
  return true;
#else
  std::ifstream file(sceneFilePath(), std::ios::in);
  if (!file.is_open()) return false;

  out.assign((std::istreambuf_iterator<char>(file)),
             std::istreambuf_iterator<char>());
  return !out.empty();
#endif
}

bool SceneStorageSdl::writeScene(const SceneManager& manager) {
  std::string out;
  bool ok = manager.writeSceneJson(out);
  if (!ok) return false;
  return writeScene(out);
}

bool SceneStorageSdl::readScene(SceneManager& manager) {
  std::string serialized;
  if (!readScene(serialized)) return false;
  return manager.loadScene(serialized);
}

bool SceneStorageSdl::writeScene(const std::string& data) {
  persistCurrentSceneName();
#ifdef __EMSCRIPTEN__
  std::string key = sceneKeyForStorage(currentSceneName_);
  return wasm_write_scene(key.c_str(), data.c_str()) > 0;
#else
  std::ofstream file(sceneFilePath(), std::ios::out | std::ios::trunc);
  if (!file.is_open()) return false;
  file << data;
  return file.good();
#endif
}

std::vector<std::string> SceneStorageSdl::findSceneNamesOnDisk() const {
#ifdef __EMSCRIPTEN__
  return {};
#else
  std::vector<std::string> names;
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(fs::current_path(), ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    const fs::path& path = entry.path();
    const std::string filename = path.filename().string();
    const size_t autoLen = std::strlen(kAutoSceneExtension);
    if (filename.size() >= autoLen &&
        filename.compare(filename.size() - autoLen, autoLen,
                         kAutoSceneExtension) == 0) {
      continue;
    }
    if (path.extension() == kSceneExtension) {
      names.push_back(path.stem().string());
    }
  }
  return names;
#endif
}

std::vector<std::string> SceneStorageSdl::findSceneNamesLocalStorage() const {
#ifdef __EMSCRIPTEN__
  std::vector<std::string> names;
  int length = wasm_list_scene_names(nullptr, 0);
  if (length <= 0) return names;
  std::string buffer;
  buffer.resize(static_cast<size_t>(length));
  int count = wasm_list_scene_names(buffer.data(), length);
  if (count < 0) return names;

  std::stringstream stream(buffer);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) names.push_back(normalizeSceneName(line));
  }
  return names;
#else
  return {};
#endif
}

std::vector<std::string> SceneStorageSdl::getAvailableSceneNames() const {
#ifdef __EMSCRIPTEN__
  std::vector<std::string> names = findSceneNamesLocalStorage();
#else
  std::vector<std::string> names = findSceneNamesOnDisk();
#endif
  if (names.empty()) names.push_back(currentSceneName_);
  return names;
}

std::string SceneStorageSdl::getCurrentSceneName() const {
  return currentSceneName_;
}

bool SceneStorageSdl::setCurrentSceneName(const std::string& name) {
  const std::string normalized = normalizeSceneName(name);
  const std::string previous = currentSceneName_;
  if (normalized == previous) {
    return PatternPagingService::setProjectName(previous) &&
           persistCurrentSceneName();
  }

  bool sceneAlreadyExists = false;
#ifdef __EMSCRIPTEN__
  const std::string targetKey = sceneKeyForStorage(normalized);
  sceneAlreadyExists = wasm_read_scene(targetKey.c_str(), nullptr, 0) > 0;
#else
  sceneAlreadyExists = std::filesystem::exists(normalized + kSceneExtension);
#endif

  if (!sceneAlreadyExists &&
      !PatternPagingService::copyProjectPages(previous, normalized)) {
    return false;
  }

  currentSceneName_ = normalized;
  if (!PatternPagingService::setProjectName(currentSceneName_) ||
      !persistCurrentSceneName()) {
    currentSceneName_ = previous;
    PatternPagingService::setProjectName(previous);
    persistCurrentSceneName();
    return false;
  }
  return true;
}

bool SceneStorageSdl::writeSceneAuto(const SceneManager& manager) {
  std::string serialized;
  if (!manager.writeSceneJson(serialized)) return false;
#ifdef __EMSCRIPTEN__
  const std::string key = autoSceneKeyForStorage();
  return wasm_write_scene(key.c_str(), serialized.c_str()) > 0;
#else
  std::ofstream file(autoSceneFilePath(), std::ios::out | std::ios::trunc);
  if (!file.is_open()) return false;
  file << serialized;
  return file.good();
#endif
}

bool SceneStorageSdl::readSceneAuto(SceneManager& manager) {
  std::string serialized;
#ifdef __EMSCRIPTEN__
  const std::string key = autoSceneKeyForStorage();
  const int length = wasm_read_scene(key.c_str(), nullptr, 0);
  if (length <= 0) return false;
  serialized.resize(static_cast<size_t>(length));
  const int written = wasm_read_scene(key.c_str(), serialized.data(), length);
  if (written <= 0) return false;
  serialized.resize(static_cast<size_t>(written));
#else
  std::ifstream file(autoSceneFilePath(), std::ios::in);
  if (!file.is_open()) return false;
  serialized.assign(std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>());
#endif
  return !serialized.empty() && manager.loadScene(serialized);
}

bool SceneStorageSdl::hasSceneAuto() const {
#ifdef __EMSCRIPTEN__
  const std::string key = autoSceneKeyForStorage();
  return wasm_read_scene(key.c_str(), nullptr, 0) > 0;
#else
  return std::filesystem::exists(autoSceneFilePath());
#endif
}

bool SceneStorageSdl::clearSceneAuto() {
#ifdef __EMSCRIPTEN__
  const std::string key = autoSceneKeyForStorage();
  return wasm_remove_scene(key.c_str()) > 0;
#else
  std::error_code error;
  std::filesystem::remove(autoSceneFilePath(), error);
  return !error && !std::filesystem::exists(autoSceneFilePath());
#endif
}
