#pragma once

// Logging layer for both Arduino (ESP32) and Desktop/SDL builds.
// Usage:
//   LOG_DEBUG("value=%d\n", x);
//   LOG_PRINTLN("hello");

#if defined(ARDUINO)
  #include <Arduino.h>

  inline void log_println(const char* msg) {
    Serial.println(msg);
  }

  template <typename... Args>
  inline void log_debug(const char* fmt, Args... args) {
    Serial.printf(fmt, args...);
  }

  #define LOG_PRINTLN(msg) ::log_println(msg)
  #define LOG_DEBUG(...)   ::log_debug(__VA_ARGS__)

  inline void logMem() {
    Serial.printf("heapFree=%u heapMin=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    Serial.printf("psramSize=%u psramFree=%u\n", ESP.getPsramSize(), ESP.getFreePsram());
  }

#else
  #include <cstdio>

  inline void log_println(const char* msg) {
    std::printf("%s\n", msg);
  }

  template <typename... Args>
  inline void log_debug(const char* fmt, Args... args) {
    std::printf(fmt, args...);
  }

  #define LOG_PRINTLN(msg) ::log_println(msg)
  #define LOG_DEBUG(...)   ::log_debug(__VA_ARGS__)
  
  inline void logMem() {
      // Desktop stub
      std::printf("[Mem] Heap monitoring not implemented for SDL build.\n");
  }
#endif
