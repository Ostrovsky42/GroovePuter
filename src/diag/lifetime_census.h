#pragma once
#ifndef GROOVEPUTER_SRC_DIAG_LIFETIME_CENSUS_H
#define GROOVEPUTER_SRC_DIAG_LIFETIME_CENSUS_H

#ifndef GROOVEPUTER_DIAG_LIFETIME_CENSUS
#define GROOVEPUTER_DIAG_LIFETIME_CENSUS 0
#endif

#if GROOVEPUTER_DIAG_LIFETIME_CENSUS

#if !defined(ARDUINO_ARCH_ESP32)
#error "GROOVEPUTER_DIAG_LIFETIME_CENSUS requires the ESP32 target"
#endif

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace GroovePuterDiag {

struct LifetimeCensusSnapshot {
  uint32_t freeInternal = 0;
  uint32_t largestInternal = 0;
  uint32_t minimumFreeInternal = 0;
  UBaseType_t taskHwmNative = 0;
};

inline LifetimeCensusSnapshot captureLifetimeCensus(
    TaskHandle_t task = nullptr) {
  LifetimeCensusSnapshot snapshot{};
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  snapshot.freeInternal = heap_caps_get_free_size(caps);
  snapshot.largestInternal = heap_caps_get_largest_free_block(caps);
  snapshot.minimumFreeInternal = heap_caps_get_minimum_free_size(caps);
  snapshot.taskHwmNative = uxTaskGetStackHighWaterMark(task);
  return snapshot;
}

inline void reportLifetimeCensus(
    const char* phase,
    const char* event,
    TaskHandle_t task = nullptr) {
  const LifetimeCensusSnapshot snapshot = captureLifetimeCensus(task);
  Serial.printf(
      "[lifetime-census] phase=%s event=%s free=%lu largest=%lu min=%lu "
      "taskHwmNative=%lu\n",
      phase ? phase : "?",
      event ? event : "?",
      static_cast<unsigned long>(snapshot.freeInternal),
      static_cast<unsigned long>(snapshot.largestInternal),
      static_cast<unsigned long>(snapshot.minimumFreeInternal),
      static_cast<unsigned long>(snapshot.taskHwmNative));
}

}  // namespace GroovePuterDiag

#define LIFETIME_CENSUS_POINT(phase, event) \
  do { GroovePuterDiag::reportLifetimeCensus((phase), (event)); } while (0)

#define LIFETIME_CENSUS_TASK_POINT(phase, event, task) \
  do { GroovePuterDiag::reportLifetimeCensus((phase), (event), (task)); } while (0)

#else

#define LIFETIME_CENSUS_POINT(phase, event) do { } while (0)
#define LIFETIME_CENSUS_TASK_POINT(phase, event, task) do { } while (0)

#endif  // GROOVEPUTER_DIAG_LIFETIME_CENSUS

#endif  // GROOVEPUTER_SRC_DIAG_LIFETIME_CENSUS_H
