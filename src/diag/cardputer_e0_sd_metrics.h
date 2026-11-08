#pragma once

#include <Arduino.h>
#include <SD.h>
#include <utility>

void cardputerE0RecordSdOpen(uint32_t elapsedUs, bool succeeded);
void cardputerE0RecordSdRead(uint32_t elapsedUs, uint32_t bytes);
void cardputerE0RecordSdWrite(uint32_t elapsedUs, uint32_t bytes);
void cardputerE0RecordSceneLoad(uint32_t elapsedUs, uint32_t bytes);
void cardputerE0RecordSceneSave(uint32_t elapsedUs, uint32_t bytes);
void cardputerE0RecordSongLoaded(bool loaded);

template <typename... Args>
File cardputerE0TimedSdOpen(Args&&... args) {
  const uint32_t startedAt = micros();
  File file = SD.open(std::forward<Args>(args)...);
  cardputerE0RecordSdOpen(micros() - startedAt,
                          static_cast<bool>(file));
  return file;
}
