#include "scene_storage_cardputer.h"

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

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

namespace {
constexpr size_t kMaxSceneNamesInUi = 24;

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
  std::string path = kScenesDirectory;
  path += "/";
  path += normalizeSceneName(name);
  path += kSceneExtension;
  return path;
}

std::string SceneStorageCardputer::currentScenePath() const {
  return scenePathFor(currentSceneName_);
}

std::string SceneStorageCardputer::autoScenePathFor(const std::string& name) const {
  std::string path = kScenesDirectory;
  path += "/";
  path += normalizeSceneName(name);
  path += kAutoSceneExtension;
  return path;
}

std::string SceneStorageCardputer::currentAutoScenePath() const {
  return autoScenePathFor(currentSceneName_);
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
    
    if (!SD.exists(kScenesDirectory)) {
      Serial.printf("Creating directory: %s\n", kScenesDirectory);
      SD.mkdir(kScenesDirectory);
    }
    
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
  bool removed = SD.remove(path.c_str());
  Serial.println("Old scene file removed status:");
  Serial.println(removed);
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
  file.flush();  // Ensure all buffered data is written to SD card
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
  // ArduinoJson fallback REMOVED - it causes OOM on DRAM-only devices
  // If streaming parse fails, caller will load default scene instead
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
  if (!file) {
    Serial.printf("Failed to open file for writing: %s\n", path.c_str());
    return false;
  }

  bool ok = manager.writeSceneJson(file);
  file.flush();
  size_t bytesWritten = file.size();
  file.close();

  // Verify by reopening
  File verify = SD.open(path.c_str(), FILE_READ);
  size_t verifiedSize = verify ? verify.size() : 0;
  if (verify) verify.close();

  if (ok && bytesWritten > 0 && verifiedSize == bytesWritten) {
    Serial.printf("Streaming write succeeded to %s (total size: %zu bytes, verified: %zu)\n", path.c_str(), bytesWritten, verifiedSize);
    return true;
  } else {
    Serial.printf("Streaming write FAILED! ok=%d, written=%zu, verified=%zu\n", ok, bytesWritten, verifiedSize);
    return false;
  }
}

bool SceneStorageCardputer::writeSceneAuto(const SceneManager& manager) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  Serial.println("Writing auto-save scene to SD card...");
  std::string path = currentAutoScenePath();
  SD.remove(path.c_str());
  
  File file = SD.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to open auto-save file for writing: %s\n", path.c_str());
    return false;
  }

  bool ok = manager.writeSceneJson(file);
  file.flush();
  size_t bytesWritten = file.size();
  file.close();

  if (ok && bytesWritten > 0) {
    Serial.printf("Auto-save succeeded to %s (%zu bytes)\n", path.c_str(), bytesWritten);
    return true;
  } else {
    Serial.printf("Auto-save FAILED! ok=%d, written=%zu\n", ok, bytesWritten);
    return false;
  }
}

bool SceneStorageCardputer::readSceneAuto(SceneManager& manager) {
  if (!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  std::string autoPath = currentAutoScenePath();
  
  // Try auto-save file first
  if (SD.exists(autoPath.c_str())) {
    Serial.printf("Reading auto-save scene from SD card (%s)...\n", autoPath.c_str());
    File file = SD.open(autoPath.c_str(), FILE_READ);
    if (file) {
      Serial.println("Auto-save file opened successfully, loading scene...");
      bool ok = manager.loadSceneEvented(file);
      file.close();
      if (ok) {
        Serial.println("Auto-save read succeeded");
        return true;
      }
      Serial.println("Auto-save read failed, will try main file");
    }
  }
  
  // Fallback to main scene file
  std::string mainPath = currentScenePath();
  if (SD.exists(mainPath.c_str())) {
    Serial.printf("Reading main scene from SD card (%s)...\n", mainPath.c_str());
    File file = SD.open(mainPath.c_str(), FILE_READ);
    if (file) {
      Serial.println("Main file opened successfully, loading scene...");
      bool ok = manager.loadSceneEvented(file);
      file.close();
      Serial.printf("Main scene read %s\n", ok ? "succeeded" : "failed");
      return ok;
    }
  }
  
  Serial.println("No scene files found (auto or main)");
  return false;
}

std::vector<std::string> SceneStorageCardputer::getAvailableSceneNames() const {
  std::vector<std::string> names;
  if (!isInitialized_) return names;
  names.reserve(8);

  File root = SD.open(kScenesDirectory);
  if (!root) {
    if (!SD.exists(kScenesDirectory)) {
       SD.mkdir(kScenesDirectory);
       root = SD.open(kScenesDirectory);
    }
    if (!root) return names;
  }

  while (true) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 2048) {
      Serial.println("Scene list near OOM, truncating directly");
      break;
    }
#endif
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      std::string fileName = entry.name();
      if (!fileName.empty() && fileName.front() == '/') fileName.erase(0, 1);
      if (endsWith(fileName, kSceneExtension)) {
        fileName.resize(fileName.size() - std::strlen(kSceneExtension));
        if (names.size() < kMaxSceneNamesInUi) {
          try {
            names.emplace_back(std::move(fileName));
          } catch (const std::bad_alloc&) {
            Serial.println("Scene list OOM, truncating");
            entry.close();
            break;
          }
        }
      }
    }
    entry.close();
  }
  root.close();
  if (names.empty()) {
    try {
      names.emplace_back(currentSceneName_);
    } catch (const std::bad_alloc&) {
      // Keep empty list on extreme memory pressure.
    }
  }
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
