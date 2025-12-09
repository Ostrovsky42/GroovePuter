#include "scene_storage_cardputer.h"
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>
#include "scenes.h"

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

void SceneStorageCardputer::initializeStorage() {
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    // Print a message if the SD card initialization fails, or if the SD card does not exist.
    // 如果SD卡初始化失败或者SD卡不存在，则打印消息。
    Serial.println("Card failed, or not present");
    isInitialized_ = false;
  }else{
    Serial.println("Card initialized successfully");
    isInitialized_ = true;
  }
}

bool SceneStorageCardputer::readScene(std::string& out) {
  if(!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  Serial.println("Reading scene from SD card...");
  File file = SD.open(kScenePath, FILE_READ);
  if (!file) return false;
  Serial.println("File opened successfully, reading data...");

  out.clear();
  while (file.available()) {
    int c = file.read();
    if (c < 0) break;
    out.push_back(static_cast<char>(c));
  }
  Serial.printf("Read %zu bytes from file: %s\n", out.size(), kScenePath);

  file.close();
  Serial.println("File read complete.");
  return !out.empty();
}

bool SceneStorageCardputer::writeScene(const std::string& data) {
  if(!isInitialized_) {
    Serial.println("Storage not initialized. Please call initializeStorage() first.");
    return false;
  }
  Serial.println("Writing scene to SD card...");
  Serial.println("Removing old scene file if it exists...");
  Serial.flush();
  SD.remove(kScenePath);
  Serial.println("Old scene file removed.");
  Serial.flush();

  Serial.println("Opening file for writing...");
  File file = SD.open(kScenePath, FILE_WRITE);
  if (!file) return false;
  Serial.println("File opened successfully, writing data...");

  Serial.printf("Data size: %zu bytes\n", data.size());
  Serial.printf("Writing to file: %s\n", kScenePath);
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
  Serial.println("Reading scene (streaming) from SD card...");
  File file = SD.open(kScenePath, FILE_READ);
  if (!file) return false;

  Serial.println("File opened successfully, loading scene...");
  bool ok = manager.loadSceneEvented(file);
  file.close();
  if (!ok) {
    Serial.println("Evented parse failed, retrying with ArduinoJson...");
    File retry = SD.open(kScenePath, FILE_READ);
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
  SD.remove(kScenePath);
  File file = SD.open(kScenePath, FILE_WRITE);
  if (!file) return false;

  bool ok = manager.writeSceneJson(file);
  file.close();
  Serial.printf("Streaming write %s\n", ok ? "succeeded" : "failed");
  return ok;
}
