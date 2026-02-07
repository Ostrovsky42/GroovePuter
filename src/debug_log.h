#pragma once

// ============================================================================
// MiniAcid Debug Logging System
// ============================================================================
//
// Usage:
//   LOG_DEBUG_UI("Cursor moved to row %d, col %d", row, col);
//   LOG_INFO_DSP("Pattern generated: %d notes", noteCount);
//   LOG_WARN_SCENE("Song length exceeds max: %d", len);
//   LOG_ERROR_UI("Invalid track index: %d", idx);
//
// Enable/disable per module by defining before including this file:
//   #define DEBUG_UI_ENABLED 1
//   #define DEBUG_DSP_ENABLED 0
//

// ============================================================================
// Global Debug Control
// ============================================================================

// Master switch - comment out to disable ALL logging
#define DEBUG_ENABLED 1

// Default log level (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)
#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL 4
#endif

// ============================================================================
// Module-specific Debug Flags
// ============================================================================

#ifndef DEBUG_UI_ENABLED
  #define DEBUG_UI_ENABLED 1      // UI pages, widgets, navigation
#endif

#ifndef DEBUG_DSP_ENABLED
  #define DEBUG_DSP_ENABLED 0     // Audio processing, synthesis
#endif

#ifndef DEBUG_SCENE_ENABLED
  #define DEBUG_SCENE_ENABLED 1   // Scene management, serialization
#endif

#ifndef DEBUG_INPUT_ENABLED
  #define DEBUG_INPUT_ENABLED 1   // Keyboard, encoder input
#endif

#ifndef DEBUG_AUDIO_ENABLED
  #define DEBUG_AUDIO_ENABLED 0   // Audio I/O, buffers
#endif

#ifndef DEBUG_PATTERN_ENABLED
  #define DEBUG_PATTERN_ENABLED 1 // Pattern generation, editing
#endif

// ============================================================================
// Color Codes for Serial Terminal (ANSI)
// ============================================================================

#ifdef ARDUINO
  // ESP32 Serial supports ANSI colors
  #define ANSI_RESET   "\033[0m"
  #define ANSI_RED     "\033[31m"
  #define ANSI_GREEN   "\033[32m"
  #define ANSI_YELLOW  "\033[33m"
  #define ANSI_BLUE    "\033[34m"
  #define ANSI_MAGENTA "\033[35m"
  #define ANSI_CYAN    "\033[36m"
  #define ANSI_GRAY    "\033[90m"
#else
  // Desktop - use full ANSI
  #define ANSI_RESET   "\033[0m"
  #define ANSI_RED     "\033[31m"
  #define ANSI_GREEN   "\033[32m"
  #define ANSI_YELLOW  "\033[33m"
  #define ANSI_BLUE    "\033[34m"
  #define ANSI_MAGENTA "\033[35m"
  #define ANSI_CYAN    "\033[36m"
  #define ANSI_GRAY    "\033[90m"
#endif

// ============================================================================
// Core Logging Macros
// ============================================================================

#if defined(DEBUG_ENABLED) && DEBUG_ENABLED

  #ifdef ARDUINO
    #define LOG_PRINTF Serial.printf
  #else
    #include <cstdio>
    #define LOG_PRINTF printf
  #endif

  // Timestamp helper (millis on Arduino, time on desktop)
  #ifdef ARDUINO
    #define LOG_TIMESTAMP() millis()
  #else
    #include <chrono>
    inline unsigned long LOG_TIMESTAMP() {
      using namespace std::chrono;
      return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
      ).count();
    }
  #endif

  // Base log macro with color and timestamp
  #define LOG_BASE(level, color, module, fmt, ...) \
    LOG_PRINTF("%s[%7lu][%s][%s] " fmt ANSI_RESET "\n", \
               color, LOG_TIMESTAMP(), level, module, ##__VA_ARGS__)

#else
  // Logging disabled - all macros become no-ops
  #define LOG_BASE(level, color, module, fmt, ...)
#endif

// ============================================================================
// Level-specific Macros
// ============================================================================

#if DEBUG_LEVEL >= 4
  #define LOG_DEBUG(module, fmt, ...) \
    LOG_BASE("DEBUG", ANSI_GRAY, module, fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(module, fmt, ...)
#endif

#if DEBUG_LEVEL >= 3
  #define LOG_INFO(module, fmt, ...) \
    LOG_BASE("INFO ", ANSI_CYAN, module, fmt, ##__VA_ARGS__)
#else
  #define LOG_INFO(module, fmt, ...)
#endif

#if DEBUG_LEVEL >= 2
  #define LOG_WARN(module, fmt, ...) \
    LOG_BASE("WARN ", ANSI_YELLOW, module, fmt, ##__VA_ARGS__)
#else
  #define LOG_WARN(module, fmt, ...)
#endif

#if DEBUG_LEVEL >= 1
  #define LOG_ERROR(module, fmt, ...) \
    LOG_BASE("ERROR", ANSI_RED, module, fmt, ##__VA_ARGS__)
#else
  #define LOG_ERROR(module, fmt, ...)
#endif

// Success messages (always green, level INFO)
#if DEBUG_LEVEL >= 3
  #define LOG_SUCCESS(module, fmt, ...) \
    LOG_BASE("OK   ", ANSI_GREEN, module, fmt, ##__VA_ARGS__)
#else
  #define LOG_SUCCESS(module, fmt, ...)
#endif

// ============================================================================
// Module-specific Convenience Macros
// ============================================================================

// --- UI Module ---
#if DEBUG_UI_ENABLED
  #define LOG_DEBUG_UI(fmt, ...)   LOG_DEBUG("UI", fmt, ##__VA_ARGS__)
  #define LOG_INFO_UI(fmt, ...)    LOG_INFO("UI", fmt, ##__VA_ARGS__)
  #define LOG_WARN_UI(fmt, ...)    LOG_WARN("UI", fmt, ##__VA_ARGS__)
  #define LOG_ERROR_UI(fmt, ...)   LOG_ERROR("UI", fmt, ##__VA_ARGS__)
  #define LOG_SUCCESS_UI(fmt, ...) LOG_SUCCESS("UI", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG_UI(fmt, ...)
  #define LOG_INFO_UI(fmt, ...)
  #define LOG_WARN_UI(fmt, ...)
  #define LOG_ERROR_UI(fmt, ...)
  #define LOG_SUCCESS_UI(fmt, ...)
#endif

// --- DSP Module ---
#if DEBUG_DSP_ENABLED
  #define LOG_DEBUG_DSP(fmt, ...)   LOG_DEBUG("DSP", fmt, ##__VA_ARGS__)
  #define LOG_INFO_DSP(fmt, ...)    LOG_INFO("DSP", fmt, ##__VA_ARGS__)
  #define LOG_WARN_DSP(fmt, ...)    LOG_WARN("DSP", fmt, ##__VA_ARGS__)
  #define LOG_ERROR_DSP(fmt, ...)   LOG_ERROR("DSP", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG_DSP(fmt, ...)
  #define LOG_INFO_DSP(fmt, ...)
  #define LOG_WARN_DSP(fmt, ...)
  #define LOG_ERROR_DSP(fmt, ...)
#endif

// --- Scene Module ---
#if DEBUG_SCENE_ENABLED
  #define LOG_DEBUG_SCENE(fmt, ...)   LOG_DEBUG("SCENE", fmt, ##__VA_ARGS__)
  #define LOG_INFO_SCENE(fmt, ...)    LOG_INFO("SCENE", fmt, ##__VA_ARGS__)
  #define LOG_WARN_SCENE(fmt, ...)    LOG_WARN("SCENE", fmt, ##__VA_ARGS__)
  #define LOG_ERROR_SCENE(fmt, ...)   LOG_ERROR("SCENE", fmt, ##__VA_ARGS__)
  #define LOG_SUCCESS_SCENE(fmt, ...) LOG_SUCCESS("SCENE", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG_SCENE(fmt, ...)
  #define LOG_INFO_SCENE(fmt, ...)
  #define LOG_WARN_SCENE(fmt, ...)
  #define LOG_ERROR_SCENE(fmt, ...)
  #define LOG_SUCCESS_SCENE(fmt, ...)
#endif

// --- Input Module ---
#if DEBUG_INPUT_ENABLED
  #define LOG_DEBUG_INPUT(fmt, ...)   LOG_DEBUG("INPUT", fmt, ##__VA_ARGS__)
  #define LOG_INFO_INPUT(fmt, ...)    LOG_INFO("INPUT", fmt, ##__VA_ARGS__)
  #define LOG_WARN_INPUT(fmt, ...)    LOG_WARN("INPUT", fmt, ##__VA_ARGS__)
  #define LOG_ERROR_INPUT(fmt, ...)   LOG_ERROR("INPUT", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG_INPUT(fmt, ...)
  #define LOG_INFO_INPUT(fmt, ...)
  #define LOG_WARN_INPUT(fmt, ...)
  #define LOG_ERROR_INPUT(fmt, ...)
#endif

// --- Audio Module ---
#if DEBUG_AUDIO_ENABLED
  #define LOG_DEBUG_AUDIO(fmt, ...)   LOG_DEBUG("AUDIO", fmt, ##__VA_ARGS__)
  #define LOG_INFO_AUDIO(fmt, ...)    LOG_INFO("AUDIO", fmt, ##__VA_ARGS__)
  #define LOG_WARN_AUDIO(fmt, ...)    LOG_WARN("AUDIO", fmt, ##__VA_ARGS__)
  #define LOG_ERROR_AUDIO(fmt, ...)   LOG_ERROR("AUDIO", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG_AUDIO(fmt, ...)
  #define LOG_INFO_AUDIO(fmt, ...)
  #define LOG_WARN_AUDIO(fmt, ...)
  #define LOG_ERROR_AUDIO(fmt, ...)
#endif

// --- Pattern Module ---
#if DEBUG_PATTERN_ENABLED
  #define LOG_DEBUG_PATTERN(fmt, ...)   LOG_DEBUG("PATTERN", fmt, ##__VA_ARGS__)
  #define LOG_INFO_PATTERN(fmt, ...)    LOG_INFO("PATTERN", fmt, ##__VA_ARGS__)
  #define LOG_WARN_PATTERN(fmt, ...)    LOG_WARN("PATTERN", fmt, ##__VA_ARGS__)
  #define LOG_ERROR_PATTERN(fmt, ...)   LOG_ERROR("PATTERN", fmt, ##__VA_ARGS__)
  #define LOG_SUCCESS_PATTERN(fmt, ...) LOG_SUCCESS("PATTERN", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG_PATTERN(fmt, ...)
  #define LOG_INFO_PATTERN(fmt, ...)
  #define LOG_WARN_PATTERN(fmt, ...)
  #define LOG_ERROR_PATTERN(fmt, ...)
  #define LOG_SUCCESS_PATTERN(fmt, ...)
#endif

// ============================================================================
// Utility Macros
// ============================================================================

// Log function entry/exit (DEBUG level)
#if DEBUG_LEVEL >= 4
  #define LOG_FUNC_ENTRY(module) \
    LOG_DEBUG(module, "→ %s()", __FUNCTION__)
  #define LOG_FUNC_EXIT(module) \
    LOG_DEBUG(module, "← %s()", __FUNCTION__)
#else
  #define LOG_FUNC_ENTRY(module)
  #define LOG_FUNC_EXIT(module)
#endif

// Conditional logging
#define LOG_IF(condition, macro, ...) \
  do { if (condition) { macro(__VA_ARGS__); } } while(0)

// Hexadecimal dump (for debugging binary data)
#if DEBUG_LEVEL >= 4
  #define LOG_HEX(module, data, len) \
    do { \
      LOG_DEBUG(module, "Hex dump (%d bytes):", (int)(len)); \
      for (int i = 0; i < (int)(len); i++) { \
        LOG_PRINTF("%02X ", ((uint8_t*)(data))[i]); \
        if ((i + 1) % 16 == 0) LOG_PRINTF("\n"); \
      } \
      if ((int)(len) % 16 != 0) LOG_PRINTF("\n"); \
    } while(0)
#else
  #define LOG_HEX(module, data, len)
#endif

// ============================================================================
// Performance Profiling
// ============================================================================

#if DEBUG_LEVEL >= 4
  struct ScopedTimer {
    const char* name;
    const char* module;
    unsigned long start;

    ScopedTimer(const char* m, const char* n) : name(n), module(m) {
      start = LOG_TIMESTAMP();
      LOG_DEBUG(module, "⏱ %s START", name);
    }

    ~ScopedTimer() {
      unsigned long elapsed = LOG_TIMESTAMP() - start;
      LOG_DEBUG(module, "⏱ %s DONE (%lu ms)", name, elapsed);
    }
  };

  #define LOG_TIMER(module, name) ScopedTimer _timer_##__LINE__(module, name)
#else
  #define LOG_TIMER(module, name)
#endif

// ============================================================================
// Example Usage
// ============================================================================
/*

#include "debug_log.h"

void SongPage::assignPattern(int patternIdx) {
  LOG_FUNC_ENTRY("UI");

  int row = cursorRow();
  int col = cursorTrack();

  LOG_DEBUG_UI("assignPattern: row=%d, col=%d, patternIdx=%d",
               row, col, patternIdx);

  if (col < 0 || col > 3) {
    LOG_ERROR_UI("Invalid column: %d", col);
    return false;
  }

  // ... do work ...

  LOG_SUCCESS_UI("Pattern %d assigned to (%d,%d)", patternIdx, row, col);
  LOG_FUNC_EXIT("UI");
  return true;
}

void MiniAcid::generateAudioBlock() {
  LOG_TIMER("DSP", "generateAudioBlock");

  // ... expensive operation ...

  LOG_DEBUG_DSP("Generated %d samples", kBlockFrames);
}

*/

// ============================================================================
// Configuration Quick Presets
// ============================================================================

// Preset: Debug UI only
// #define DEBUG_PRESET_UI_ONLY
#ifdef DEBUG_PRESET_UI_ONLY
  #undef DEBUG_UI_ENABLED
  #define DEBUG_UI_ENABLED 1
  #undef DEBUG_DSP_ENABLED
  #define DEBUG_DSP_ENABLED 0
  #undef DEBUG_SCENE_ENABLED
  #define DEBUG_SCENE_ENABLED 0
  #undef DEBUG_INPUT_ENABLED
  #define DEBUG_INPUT_ENABLED 1
  #undef DEBUG_AUDIO_ENABLED
  #define DEBUG_AUDIO_ENABLED 0
  #undef DEBUG_PATTERN_ENABLED
  #define DEBUG_PATTERN_ENABLED 0
#endif

// Preset: Performance profiling
// #define DEBUG_PRESET_PERFORMANCE
#ifdef DEBUG_PRESET_PERFORMANCE
  #undef DEBUG_LEVEL
  #define DEBUG_LEVEL 4
  #undef DEBUG_DSP_ENABLED
  #define DEBUG_DSP_ENABLED 1
  #undef DEBUG_AUDIO_ENABLED
  #define DEBUG_AUDIO_ENABLED 1
#endif

// Preset: Silent (production)
// #define DEBUG_PRESET_SILENT
#ifdef DEBUG_PRESET_SILENT
  #undef DEBUG_ENABLED
  #undef DEBUG_LEVEL
  #define DEBUG_LEVEL 0
#endif
