#include "scene_storage_cardputer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <new>
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

#include "scenes.h"
#include "src/audio/pattern_paging.h"
#include "src/platform/cardputer_sd.h"

namespace {
constexpr size_t kMaxSceneNamesInUi = 24;
constexpr size_t kMaxSceneNameLength = 48;
constexpr const char* kTempSuffix = ".tmp";
constexpr const char* kBackupSuffix = ".bak";

bool endsWith(const std::string& value, const char* suffix) {
  const size_t suffixLen = std::strlen(suffix);
  return value.size() >= suffixLen &&
         value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
}

std::string trimWhitespace(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string siblingPath(const std::string& path, const char* suffix) {
  return path + suffix;
}

bool verifyFileSize(const std::string& path, size_t expectedSize) {
  File verify = SD.open(path.c_str(), FILE_READ);
  if (!verify) return false;
  const size_t actualSize = verify.size();
  verify.close();
  return expectedSize > 0 && actualSize == expectedSize;
}

bool commitTempFile(const std::string& path, const std::string& tempPath) {
  const std::string backupPath = siblingPath(path, kBackupSuffix);
  SD.remove(backupPath.c_str());

  const bool hadMain = SD.exists(path.c_str());
  if (hadMain && !SD.rename(path.c_str(), backupPath.c_str())) {
    Serial.printf("Failed to move current scene to backup: %s\n", path.c_str());
    SD.remove(tempPath.c_str());
    return false;
  }

  if (!SD.rename(tempPath.c_str(), path.c_str())) {
    Serial.printf("Failed to promote temporary scene: %s\n", tempPath.c_str());
    if (hadMain) {
      SD.rename(backupPath.c_str(), path.c_str());
    }
    SD.remove(tempPath.c_str());
    return false;
  }

  return true;
}

File openMainOrBackup(const std::string& path) {
  File file = SD.open(path.c_str(), FILE_READ);
  if (file) return file;

  const std::string backupPath = siblingPath(path, kBackupSuffix);
  return SD.open(backupPath.c_str(), FILE_READ);
}

}  // namespace

std::string SceneStorageCardputer::normalizeSceneName(const std::string& name) const {
  std::string source = trimWhitespace(name);
  if (endsWith(source, kSceneExtension)) {
    source.resize(source.size() - std::strlen(kSceneExtension));
  }
  if (endsWith(source, kAutoSceneExtension)) {
    source.resize(source.size() - std::strlen(kAutoSceneExtension));
  }

  std::string cleaned;
  cleaned.reserve(std::min(source.size(), kMaxSceneNameLength));
  bool previousWasSeparator = false;

  for (char raw : source) {
    if (cleaned.size() >= kMaxSceneNameLength) break;
    const unsigned char ch = static_cast<unsigned char>(raw);
    const bool allowed = std::isalnum(ch) || raw == '-' || raw == '_' || raw == ' ';
    const char output = allowed ? raw : '_';
    const bool isSeparator = output == '_' || output == ' ';
    if (isSeparator && previousWasSeparator) continue;
    cleaned.push_back(output);
    previousWasSeparator = isSeparator;
  }

  while (!cleaned.empty() && (cleaned.front() == ' ' || cleaned.front() == '_')) {
    cleaned.erase(cleaned.begin());
  }
  while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '_')) {
    cleaned.pop_back();
  }

  if (cleaned.empty() || cleaned == "." || cleaned == "..") {
    cleaned = kDefaultSceneName;
  }
  return cleaned;
}

std::string SceneStorageCardputer::scenePathFor(const std::string& name) const {
  return std::string(kScenesDirectory) + "/" + normalizeSceneName(name) +
         kSceneExtension;
}

std::string SceneStorageCardputer::currentScenePath() const {
  return scenePathFor(currentSceneName_);
}

std::string SceneStorageCardputer::autoScenePathFor(const std::string& name) const {
  return std::string(kScenesDirectory) + "/" + normalizeSceneName(name) +
         kAutoSceneExtension;
}

std::string SceneStorageCardputer::currentAutoScenePath() const {
  return autoScenePathFor(currentSceneName_);
}

void SceneStorageCardputer::loadStoredSceneName() {
  if (!isInitialized_) return;
  File file = openMainOrBackup(kSceneNamePath);
  if (!file) return;

  std::string storedName;
  while (file.available()) {
    const int c = file.read();
    if (c < 0) break;
    storedName.push_back(static_cast<char>(c));
  }
  file.close();

  storedName = trimWhitespace(storedName);
  if (!storedName.empty()) {
    currentSceneName_ = normalizeSceneName(storedName);
  }
}

bool SceneStorageCardputer::persistCurrentSceneName() const {
  if (!isInitialized_) return false;

  const std::string path = kSceneNamePath;
  const std::string tempPath = siblingPath(path, kTempSuffix);
  SD.remove(tempPath.c_str());

  File file = SD.open(tempPath.c_str(), FILE_WRITE);
  if (!file) return false;
  const size_t written = file.print(currentSceneName_.c_str());
  file.flush();
  file.close();

  if (written != currentSceneName_.size() || !verifyFileSize(tempPath, written)) {
    SD.remove(tempPath.c_str());
    return false;
  }
  return commitTempFile(path, tempPath);
}

void SceneStorageCardputer::initializeStorage() {
  if (isInitialized_ && GroovePuterPlatform::cardputerSdMounted()) return;

  if (!GroovePuterPlatform::ensureCardputerSdMounted()) {
    Serial.println("Card failed, or not present");
    isInitialized_ = false;
    return;
  }

  Serial.println("Card initialized successfully");
  isInitialized_ = true;

  if (!SD.exists(kScenesDirectory) && !SD.mkdir(kScenesDirectory)) {
    Serial.printf("Failed to create directory: %s\n", kScenesDirectory);
    isInitialized_ = false;
    return;
  }

  loadStoredSceneName();
  if (!PatternPagingService::setProjectName(currentSceneName_)) {
    Serial.printf("Failed to select pattern namespace: %s\n",
                  currentSceneName_.c_str());
  }
}

bool SceneStorageCardputer::readScene(std::string& out) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }

  const std::string path = currentScenePath();
  File file = openMainOrBackup(path);
  if (!file) return false;

  out.clear();
  while (file.available()) {
    const int c = file.read();
    if (c < 0) break;
    out.push_back(static_cast<char>(c));
  }
  file.close();
  return !out.empty();
}

bool SceneStorageCardputer::writeScene(const std::string& data) {
  if (!isInitialized_ || data.empty()) return false;

  const std::string path = currentScenePath();
  const std::string tempPath = siblingPath(path, kTempSuffix);
  SD.remove(tempPath.c_str());

  File file = SD.open(tempPath.c_str(), FILE_WRITE);
  if (!file) return false;
  const size_t written = file.write(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());
  file.flush();
  file.close();

  if (written != data.size() || !verifyFileSize(tempPath, written)) {
    SD.remove(tempPath.c_str());
    return false;
  }
  if (!commitTempFile(path, tempPath)) return false;
  return persistCurrentSceneName();
}

bool SceneStorageCardputer::readScene(SceneManager& manager) {
  if (!isInitialized_) return false;

  const std::string path = currentScenePath();
  File file = SD.open(path.c_str(), FILE_READ);
  if (file) {
    const bool ok = manager.loadSceneEvented(file);
    file.close();
    if (ok) return true;
    Serial.printf("Main scene is invalid, trying backup: %s\n", path.c_str());
  }

  const std::string backupPath = siblingPath(path, kBackupSuffix);
  File backup = SD.open(backupPath.c_str(), FILE_READ);
  if (!backup) return false;
  const bool recovered = manager.loadSceneEvented(backup);
  backup.close();
  return recovered;
}

bool SceneStorageCardputer::writeScene(const SceneManager& manager) {
  if (!isInitialized_) return false;

  const std::string path = currentScenePath();
  const std::string tempPath = siblingPath(path, kTempSuffix);
  SD.remove(tempPath.c_str());

  File file = SD.open(tempPath.c_str(), FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to open temporary scene: %s\n", tempPath.c_str());
    return false;
  }

  const bool serialized = manager.writeSceneJson(file);
  file.flush();
  const size_t bytesWritten = file.size();
  file.close();

  if (!serialized || !verifyFileSize(tempPath, bytesWritten)) {
    Serial.printf("Scene serialization failed: ok=%d bytes=%zu\n",
                  serialized, bytesWritten);
    SD.remove(tempPath.c_str());
    return false;
  }
  if (!commitTempFile(path, tempPath)) return false;
  if (!persistCurrentSceneName()) {
    Serial.println("Scene data saved, but current-scene pointer update failed");
    return false;
  }

  Serial.printf("Scene saved transactionally: %s (%zu bytes)\n",
                path.c_str(), bytesWritten);
  return true;
}

bool SceneStorageCardputer::writeSceneAuto(const SceneManager& manager) {
  if (!isInitialized_) return false;

  const std::string path = currentAutoScenePath();
  const std::string tempPath = siblingPath(path, kTempSuffix);
  SD.remove(tempPath.c_str());

  File file = SD.open(tempPath.c_str(), FILE_WRITE);
  if (!file) return false;
  const bool serialized = manager.writeSceneJson(file);
  file.flush();
  const size_t bytesWritten = file.size();
  file.close();

  if (!serialized || !verifyFileSize(tempPath, bytesWritten)) {
    SD.remove(tempPath.c_str());
    return false;
  }
  return commitTempFile(path, tempPath);
}

bool SceneStorageCardputer::readSceneAuto(SceneManager& manager) {
  if (!isInitialized_) return false;

  const std::string autoPath = currentAutoScenePath();
  File autoFile = SD.open(autoPath.c_str(), FILE_READ);
  if (autoFile) {
    const bool ok = manager.loadSceneEvented(autoFile);
    autoFile.close();
    if (ok) return true;
  }

  const std::string autoBackup = siblingPath(autoPath, kBackupSuffix);
  File autoBackupFile = SD.open(autoBackup.c_str(), FILE_READ);
  if (autoBackupFile) {
    const bool ok = manager.loadSceneEvented(autoBackupFile);
    autoBackupFile.close();
    if (ok) return true;
  }

  return false;
}

bool SceneStorageCardputer::hasSceneAuto() const {
  if (!isInitialized_) return false;
  const std::string path = currentAutoScenePath();
  return SD.exists(path.c_str()) ||
         SD.exists(siblingPath(path, kBackupSuffix).c_str());
}

bool SceneStorageCardputer::clearSceneAuto() {
  if (!isInitialized_) return false;
  const std::string path = currentAutoScenePath();
  const std::string backup = siblingPath(path, kBackupSuffix);
  const std::string temp = siblingPath(path, kTempSuffix);
  if (SD.exists(path.c_str()) && !SD.remove(path.c_str())) return false;
  if (SD.exists(backup.c_str()) && !SD.remove(backup.c_str())) return false;
  if (SD.exists(temp.c_str()) && !SD.remove(temp.c_str())) return false;
  return !SD.exists(path.c_str()) && !SD.exists(backup.c_str());
}

std::vector<std::string> SceneStorageCardputer::getAvailableSceneNames() const {
  std::vector<std::string> names;
  if (!isInitialized_) return names;
  names.reserve(8);

  File root = SD.open(kScenesDirectory);
  if (!root) return names;

  while (true) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 2048) {
      Serial.println("Scene list near OOM, truncating");
      break;
    }
#endif
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      std::string fileName = entry.name();
      if (!fileName.empty() && fileName.front() == '/') fileName.erase(0, 1);
      if (!endsWith(fileName, kAutoSceneExtension) &&
          endsWith(fileName, kSceneExtension)) {
        fileName.resize(fileName.size() - std::strlen(kSceneExtension));
        if (names.size() < kMaxSceneNamesInUi) {
          try {
            names.emplace_back(std::move(fileName));
          } catch (const std::bad_alloc&) {
            entry.close();
            break;
          }
        }
      }
    }
    entry.close();
  }
  root.close();

  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

std::string SceneStorageCardputer::getCurrentSceneName() const {
  return currentSceneName_;
}

bool SceneStorageCardputer::setCurrentSceneName(const std::string& name) {
  const std::string normalized = normalizeSceneName(name);
  if (!isInitialized_) {
    currentSceneName_ = normalized;
    PatternPagingService::setProjectName(currentSceneName_);
    return false;
  }

  const std::string previous = currentSceneName_;
  if (normalized == previous) {
    return PatternPagingService::setProjectName(previous);
  }

  const std::string targetPath = scenePathFor(normalized);
  const std::string targetBackup = siblingPath(targetPath, kBackupSuffix);
  const bool sceneAlreadyExists =
      SD.exists(targetPath.c_str()) || SD.exists(targetBackup.c_str());

  if (!sceneAlreadyExists &&
      !PatternPagingService::copyProjectPages(previous, normalized)) {
    return false;
  }

  currentSceneName_ = normalized;
  if (!PatternPagingService::setProjectName(currentSceneName_)) {
    currentSceneName_ = previous;
    PatternPagingService::setProjectName(previous);
    return false;
  }

  // Existing scenes are selections and should become the next boot target now.
  // New scene names are only committed after the scene data itself is written.
  if (!sceneAlreadyExists) return true;
  if (persistCurrentSceneName()) return true;

  currentSceneName_ = previous;
  PatternPagingService::setProjectName(previous);
  return false;
}
