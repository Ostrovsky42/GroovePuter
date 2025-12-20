#include "scene_storage_cardputer.h"

#include <cctype>
#include <cstring>
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>

#include "scenes.h"

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

namespace {

bool endsWith(const std::string& value, const char* suffix) {
  size_t suffixLen = std::strlen(suffix);
  return value.size() >= suffixLen &&
         value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
}

std::string trimWhitespace(const std::string& value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

}

std::string SceneStorageCardputer::normalizeSceneName(const std::string& name) const {
  std::string cleaned = name;
  if (cleaned.empty()) cleaned = kDefaultSceneName;
  if (!cleaned.empty() && cleaned.front() == '/') cleaned.erase(0, 1);
  if (endsWith(cleaned, kSceneExtension)) {
    cleaned.resize(cleaned.size() - std::strlen(kSceneExtension));
  }
  if (cleaned.empty()) cleaned = kDefaultSceneName;
  return cleaned;
}

std::string SceneStorageCardputer::scenePathFor(const std::string& name) const {
  std::string path = "/";
  path += normalizeSceneName(name);
  path += kSceneExtension;
  return path;
}

std::string SceneStorageCardputer::currentScenePath() const {
  return scenePathFor(currentSceneName_);
}

void SceneStorageCardputer::loadStoredSceneName() {
  if (!isInitialized_) return;
  File file = SD.open(kSceneNamePath, FILE_READ);
  if (!file) return;

  std::string storedName;
  while (file.available()) {
    int c = file.read();
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
  SD.remove(kSceneNamePath);
  File file = SD.open(kSceneNamePath, FILE_WRITE);
  if (!file) return false;
  size_t written = file.print(currentSceneName_.c_str());
  file.close();
  return written == currentSceneName_.size();
}

void SceneStorageCardputer::initializeStorage() {
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    // Print a message if the SD card initialization fails, or if the SD card does not exist.
    // 如果SD卡初始化失败或者SD卡不存在，则打印消息。
    Serial.println("Card failed, or not present");
    isInitialized_ = false;
  } else {
    Serial.println("Card initialized successfully");
    isInitialized_ = true;
    loadStoredSceneName();
  }
}

bool SceneStorageCardputer::readScene(std::string& out) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  std::string path = currentScenePath();
  Serial.printf("Reading scene from SD card (%s)...\n", path.c_str());
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return false;
  Serial.println("File opened successfully, reading data...");

  out.clear();
  while (file.available()) {
    int c = file.read();
    if (c < 0) break;
    out.push_back(static_cast<char>(c));
  }
  Serial.printf("Read %zu bytes from file: %s\n", out.size(), path.c_str());

  file.close();
  Serial.println("File read complete.");
  return !out.empty();
}

bool SceneStorageCardputer::writeScene(const std::string& data) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  Serial.println("Writing scene to SD card...");
  Serial.println("Removing old scene file if it exists...");
  Serial.flush();
  std::string path = currentScenePath();
  SD.remove(path.c_str());
  Serial.println("Old scene file removed.");
  Serial.flush();

  persistCurrentSceneName();

  Serial.println("Opening file for writing...");
  File file = SD.open(path.c_str(), FILE_WRITE);
  if (!file) return false;
  Serial.println("File opened successfully, writing data...");

  Serial.printf("Data size: %zu bytes\n", data.size());
  Serial.printf("Writing to file: %s\n", path.c_str());
  size_t written = file.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  Serial.printf("Written %zu bytes to file.\n", written);
  file.close();
  Serial.println("File write complete.");
  return written == data.size();
}

bool SceneStorageCardputer::readScene(SceneManager& manager) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  std::string path = currentScenePath();
  Serial.printf("Reading scene (streaming) from SD card (%s)...\n", path.c_str());
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return false;

  Serial.println("File opened successfully, loading scene...");
  bool ok = manager.loadSceneEvented(file);
  file.close();
  if (!ok) {
    Serial.println("Evented parse failed, retrying with ArduinoJson...");
    File retry = SD.open(path.c_str(), FILE_READ);
    if (retry) {
      ok = manager.loadSceneJson(retry);
      retry.close();
    }
  }
  Serial.printf("Streaming read %s\n", ok ? "succeeded" : "failed");
  return ok;
}

bool SceneStorageCardputer::writeScene(const SceneManager& manager) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  Serial.println("Writing scene (streaming) to SD card...");
  persistCurrentSceneName();
  std::string path = currentScenePath();
  SD.remove(path.c_str());
  File file = SD.open(path.c_str(), FILE_WRITE);
  if (!file) return false;

  bool ok = manager.writeSceneJson(file);
  file.close();
  Serial.printf("Streaming write %s\n", ok ? "succeeded" : "failed");
  return ok;
}

std::vector<std::string> SceneStorageCardputer::getAvailableSceneNames() const {
  std::vector<std::string> names;
  if (!isInitialized_) return names;

  File root = SD.open("/");
  if (!root) return names;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      std::string fileName = entry.name();
      if (!fileName.empty() && fileName.front() == '/') fileName.erase(0, 1);
      if (endsWith(fileName, kSceneExtension)) {
        fileName.resize(fileName.size() - std::strlen(kSceneExtension));
        names.push_back(fileName);
      }
    }
    entry.close();
  }
  root.close();
  if (names.empty()) names.push_back(currentSceneName_);
  return names;
}

std::string SceneStorageCardputer::getCurrentSceneName() const {
  return currentSceneName_;
}

bool SceneStorageCardputer::setCurrentSceneName(const std::string& name) {
  currentSceneName_ = normalizeSceneName(name);
  if (!isInitialized_) return false;
  return persistCurrentSceneName();
}
