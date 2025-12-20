#include "scene_storage_sdl.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <cstring>
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif

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

EM_JS(int, wasm_read_current_scene_name, (char* out, int maxLen), {
  const key = 'miniacid:scene:current';
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
  const key = 'miniacid:scene:current';
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
  const prefix = 'miniacid:scene';
  for (let i = 0; i < localStorage.length; ++i) {
    const key = localStorage.key(i);
    if (!key || !key.startsWith(prefix)) continue;
    if (key === prefix) {
      names.push('miniacid_scene');
    } else if (key.startsWith(prefix + ':')) {
      if(key.substring(prefix.length + 1) == "current") continue;
      names.push(key.substring(prefix.length + 1));
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
  static constexpr const char* kLegacyKey = "miniacid:scene";
  static constexpr const char* kKeyPrefix = "miniacid:scene:";
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
  currentSceneName_ = normalizeSceneName(name);
  return persistCurrentSceneName();
}
